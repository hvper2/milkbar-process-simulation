#include "common.h"
#include "utils.h"

static SharedState *shared_state = NULL;
static int shm_id = -1;
static int msg_queue_id = -1;
static int sem_id = -1;
static int running = 1;
static int x3_original = X3;

#define MAX_GROUPS 100
static int group_to_table_type[MAX_GROUPS];
static int group_to_table_index[MAX_GROUPS];

void sem_wait(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    
    if (semop(sem_id, &sem_op, 1) == -1) {
        if (errno != EINTR) {
            log_message("OBSLUGA: Błąd sem_wait (sem_num=%d): %s", sem_num, strerror(errno));
        }
    }
}

void sem_signal(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    
    if (semop(sem_id, &sem_op, 1) == -1) {
        log_message("OBSLUGA: Błąd sem_signal (sem_num=%d): %s", sem_num, strerror(errno));
    }
}

int find_free_table(int group_size, int *table_type, int *table_index) {
    // Stoliki 1-osobowe
    if (group_size == 1) {
        for (int i = 0; i < X1; i++) {
            if (shared_state->table_1[i] == 0) {
                *table_type = 1;
                *table_index = i;
                return 1;
            }
        }
    }
    
    // Stoliki 2-osobowe
    if (group_size <= 2) {
        for (int i = 0; i < X2; i++) {
            if (shared_state->table_2[i] == 0) {
                *table_type = 2;
                *table_index = i;
                return 1;
            }
        }
    }
    
    // Stoliki 3-osobowe
    int x3_limit = X3;
    if (shared_state->x3_doubled) {
        x3_limit = X3;
    }
    
    if (group_size <= 3) {
        for (int i = 0; i < x3_limit; i++) {
            if (shared_state->table_3[i] == 0) {
                *table_type = 3;
                *table_index = i;
                return 1;
            }
        }
    }
    
    // Stoliki 4-osobowe (mogą siadać grupy równoliczne)
    if (group_size <= 4) {
        for (int i = 0; i < X4; i++) {
            int occupied = shared_state->table_4[i];
            
            if (occupied == 0) {
                *table_type = 4;
                *table_index = i;
                return 1;
            } else if (occupied == group_size && occupied + group_size <= 4) {
                *table_type = 4;
                *table_index = i;
                return 1;
            }
        }
    }
    
    return 0;
}

void allocate_table(int table_type, int table_index, int group_size) {
    switch (table_type) {
        case 1:
            shared_state->table_1[table_index] = 1;
            break;
        case 2:
            shared_state->table_2[table_index] = group_size;
            break;
        case 3:
            shared_state->table_3[table_index] = group_size;
            break;
        case 4:
            shared_state->table_4[table_index] += group_size;
            break;
        default:
            log_message("OBSLUGA: Błąd - nieprawidłowy typ stolika: %d", table_type);
            return;
    }
    shared_state->total_free_seats -= group_size;
}

void free_table(int table_type, int table_index, int group_size) {
    switch (table_type) {
        case 1:
            shared_state->table_1[table_index] = 0;
            break;
        case 2:
            shared_state->table_2[table_index] = 0;
            break;
        case 3:
            shared_state->table_3[table_index] = 0;
            break;
        case 4:
            shared_state->table_4[table_index] -= group_size;
            if (shared_state->table_4[table_index] < 0) {
                shared_state->table_4[table_index] = 0;
            }
            break;
        default:
            log_message("OBSLUGA: Błąd - nieprawidłowy typ stolika: %d", table_type);
            return;
    }
    shared_state->total_free_seats += group_size;
}

void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        log_message("OBSLUGA: Otrzymano SIGUSR1 - próba podwojenia X3");
        sem_wait(sem_id, SEM_SHARED_STATE);
        
        if (shared_state->x3_doubled == 0) {
            shared_state->x3_doubled = 1;
            x3_original = X3;
            log_message("OBSLUGA: X3 oznaczone jako podwojone (tablica: %d stolików)", X3);
        } else {
            log_message("OBSLUGA: X3 już było oznaczone jako podwojone - ignoruję");
        }
        
        sem_signal(sem_id, SEM_SHARED_STATE);
        
    } else if (sig == SIGUSR2) {
        log_message("OBSLUGA: Otrzymano SIGUSR2 - rezerwacja miejsc");
        sem_wait(sem_id, SEM_SHARED_STATE);
        
        int reserved = 2;
        shared_state->reserved_seats += reserved;
        shared_state->total_free_seats -= reserved;
        
        log_message("OBSLUGA: Zarezerwowano %d miejsc (łącznie zarezerwowanych: %d)", 
                   reserved, shared_state->reserved_seats);
        
        sem_signal(sem_id, SEM_SHARED_STATE);
        
    } else if (sig == SIGTERM || sig == SIGINT) {
        log_message("OBSLUGA: Otrzymano sygnał %d - kończę pracę", sig);
        running = 0;
    }
}

int main(void) {
    log_message("OBSLUGA: Start pracy obsługi");
    
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    // Otwórz zasoby IPC
    shared_state = get_shared_memory();
    if (shared_state == NULL) {
        handle_error("OBSLUGA: get_shared_memory failed");
    }
    
    shm_id = shmget(SHM_KEY, 0, 0);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("OBSLUGA: get_message_queue failed");
    }
    
    sem_id = get_semaphores();
    if (sem_id == -1) {
        handle_error("OBSLUGA: get_semaphores failed");
    }
    
    log_message("OBSLUGA: Połączono z zasobami IPC");
    
    Message msg;
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    log_message("OBSLUGA: Rozpoczęcie obsługi wiadomości");
    
    for (int i = 0; i < MAX_GROUPS; i++) {
        group_to_table_type[i] = 0;
        group_to_table_index[i] = -1;
    }
    
    while (running) {
        ssize_t received = msgrcv(msg_queue_id, &msg, msg_size, 0, 0);
        
        if (received == -1) {
            if (errno == EINTR) {
                if (!running) {
                    break;
                }
                continue;
            } else {
                log_message("OBSLUGA: Błąd msgrcv: %s", strerror(errno));
                if (!running) {
                    break;
                }
                continue;
            }
        }
        
        if (msg.mtype == MSG_TYPE_PAID) {
            log_message("OBSLUGA: Otrzymano MSG_TYPE_PAID (grupa #%d, rozmiar=%d)", 
                       msg.group_id, msg.group_size);
            
            sem_wait(sem_id, SEM_SHARED_STATE);
            
            int table_type, table_index;
            if (find_free_table(msg.group_size, &table_type, &table_index)) {
                allocate_table(table_type, table_index, msg.group_size);
                
                int group_idx = msg.group_id % MAX_GROUPS;
                group_to_table_type[group_idx] = table_type;
                group_to_table_index[group_idx] = table_index;
                
                log_message("OBSLUGA: Przydzielono stolik %d-os. (indeks %d) grupie #%d", 
                           table_type, table_index, msg.group_id);
            } else {
                log_message("OBSLUGA: BŁĄD - nie znaleziono stolika dla grupy #%d!", msg.group_id);
            }
            
            sem_signal(sem_id, SEM_SHARED_STATE);
            
        } else if (msg.mtype == MSG_TYPE_DISHES) {
            log_message("OBSLUGA: Otrzymano MSG_TYPE_DISHES (grupa #%d)", msg.group_id);
            
            sem_wait(sem_id, SEM_SHARED_STATE);
            
            int group_idx = msg.group_id % MAX_GROUPS;
            int table_type = group_to_table_type[group_idx];
            int table_index = group_to_table_index[group_idx];
            
            if (table_type > 0 && table_index >= 0) {
                free_table(table_type, table_index, msg.group_size);
                shared_state->dirty_dishes += msg.group_size;
                
                log_message("OBSLUGA: Zwolniono stolik %d-os. (indeks %d) - grupa #%d", 
                           table_type, table_index, msg.group_id);
                log_message("OBSLUGA: Brudne naczynia: %d", shared_state->dirty_dishes);
                
                group_to_table_type[group_idx] = 0;
                group_to_table_index[group_idx] = -1;
            } else {
                log_message("OBSLUGA: BŁĄD - nie znaleziono stolika dla grupy #%d!", msg.group_id);
            }
            
            sem_signal(sem_id, SEM_SHARED_STATE);
        }
    }
    
    log_message("OBSLUGA: Kończenie pracy obsługi");
    
    if (shared_state != NULL) {
        if (shmdt(shared_state) == -1) {
            log_message("OBSLUGA: Błąd shmdt: %s", strerror(errno));
        }
    }
    
    return EXIT_SUCCESS;
}
