#include "common.h"
#include "utils.h"

static SharedState *shared_state = NULL;
static int msg_queue_id = -1;
static int sem_id = -1;
static int running = 1;

#define MAX_GROUPS 100
static int group_to_table_type[MAX_GROUPS];
static int group_to_table_index[MAX_GROUPS];

// Kolejka oczekujacych klientow
#define MAX_WAITING 50
typedef struct {
    int group_id;
    int group_size;
} WaitingClient;

static WaitingClient waiting_queue[MAX_WAITING];
static int waiting_count = 0;

// Funkcja semaforowa wait
static void sem_wait_op(int sem_id, int sem_num) {
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
// Funkcja semaforowa signal
static void sem_signal_op(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    
    if (semop(sem_id, &sem_op, 1) == -1) {
        log_message("OBSLUGA: Błąd sem_signal (sem_num=%d): %s", sem_num, strerror(errno));
    }
}

// Funkcja znajdująca wolne stoliki
static int find_free_table(int group_size, int *table_type, int *table_index) {
    if (group_size == 1) {
        for (int i = 0; i < X1; i++) {
            if (shared_state->table_1[i] == 0) {
                *table_type = 1;
                *table_index = i;
                return 1;
            }
        }
    }
    
    if (group_size <= 2) {
        for (int i = 0; i < X2; i++) {
            int occupied = shared_state->table_2[i];
            if (occupied == -1) continue;
            if (occupied == 0) {
                *table_type = 2;
                *table_index = i;
                return 1;
            }
            if (group_size == 1 && occupied == 1 && occupied + group_size <= 2) {
                *table_type = 2;
                *table_index = i;
                return 1;
            }
        }
    }
    
    int x3_limit = shared_state->effective_x3;
    if (group_size <= 3) {
        for (int i = 0; i < x3_limit; i++) {
            int occupied = shared_state->table_3[i];
            if (occupied == -1) continue;
            if (occupied == 0) {
                *table_type = 3;
                *table_index = i;
                return 1;
            }
            if (occupied == group_size && occupied + group_size <= 3) {
                *table_type = 3;
                *table_index = i;
                return 1;
            }
        }
    }
    
    if (group_size <= 4) {
        for (int i = 0; i < X4; i++) {
            int occupied = shared_state->table_4[i];
            if (occupied == -1) continue;
            if (occupied == 0) {
                *table_type = 4;
                *table_index = i;
                return 1;
            }
            if (occupied == group_size && occupied + group_size <= 4) {
                *table_type = 4;
                *table_index = i;
                return 1;
            }
        }
    }
    
    return 0;
}

// Funkcja alokująca stolik
static void allocate_table(int table_type, int table_index, int group_size, int group_id) {
    switch (table_type) {
        case 1:
            shared_state->table_1[table_index] = 1;
            shared_state->table_1_groups[table_index][0] = group_id;
            break;
        case 2:
            shared_state->table_2[table_index] += group_size;
            for (int i = 0, assigned = 0; i < 2 && assigned < group_size; i++) {
                if (shared_state->table_2_groups[table_index][i] == 0) {
                    shared_state->table_2_groups[table_index][i] = group_id;
                    assigned++;
                }
            }
            break;
        case 3:
            shared_state->table_3[table_index] += group_size;
            for (int i = 0, assigned = 0; i < 3 && assigned < group_size; i++) {
                if (shared_state->table_3_groups[table_index][i] == 0) {
                    shared_state->table_3_groups[table_index][i] = group_id;
                    assigned++;
                }
            }
            break;
        case 4:
            shared_state->table_4[table_index] += group_size;
            for (int i = 0, assigned = 0; i < 4 && assigned < group_size; i++) {
                if (shared_state->table_4_groups[table_index][i] == 0) {
                    shared_state->table_4_groups[table_index][i] = group_id;
                    assigned++;
                }
            }
            break;
        default:
            log_message("OBSLUGA: Błąd - nieprawidłowy typ stolika: %d", table_type);
            return;
    }
    shared_state->total_free_seats -= group_size;
}

// Funkcja zwalniająca stolik
static void free_table(int table_type, int table_index, int group_size, int group_id) {
    int is_reserved = 0;
    
    switch (table_type) {
        case 1:
            if (shared_state->table_1[table_index] == -1) {
                is_reserved = 1;
                break;
            }
            shared_state->table_1[table_index] = 0;
            shared_state->table_1_groups[table_index][0] = 0;
            break;
        case 2:
            if (shared_state->table_2[table_index] == -1) {
                is_reserved = 1;
                break;
            }
            shared_state->table_2[table_index] -= group_size;
            if (shared_state->table_2[table_index] < 0) {
                shared_state->table_2[table_index] = 0;
            }
            for (int i = 0; i < 2; i++) {
                if (shared_state->table_2_groups[table_index][i] == group_id) {
                    shared_state->table_2_groups[table_index][i] = 0;
                }
            }
            break;
        case 3:
            if (shared_state->table_3[table_index] == -1) {
                is_reserved = 1;
                break;
            }
            shared_state->table_3[table_index] -= group_size;
            if (shared_state->table_3[table_index] < 0) {
                shared_state->table_3[table_index] = 0;
            }
            for (int i = 0; i < 3; i++) {
                if (shared_state->table_3_groups[table_index][i] == group_id) {
                    shared_state->table_3_groups[table_index][i] = 0;
                }
            }
            break;
        case 4:
            if (shared_state->table_4[table_index] == -1) {
                is_reserved = 1;
                break;
            }
            shared_state->table_4[table_index] -= group_size;
            if (shared_state->table_4[table_index] < 0) {
                shared_state->table_4[table_index] = 0;
            }
            for (int i = 0; i < 4; i++) {
                if (shared_state->table_4_groups[table_index][i] == group_id) {
                    shared_state->table_4_groups[table_index][i] = 0;
                }
            }
            break;
        default:
            log_message("OBSLUGA: Błąd - nieprawidłowy typ stolika: %d", table_type);
            return;
    }
    
    if (!is_reserved) {
        shared_state->total_free_seats += group_size;
    }
}

// Funkcja synchronizująca kolejkę oczekujących klientów z pamięcią dzieloną
static void sync_waiting_queue_to_shared(void) {
    shared_state->waiting_count = waiting_count;
    for (int i = 0; i < waiting_count && i < MAX_WAITING_GROUPS; i++) {
        shared_state->waiting_group_ids[i] = waiting_queue[i].group_id;
        shared_state->waiting_group_sizes[i] = waiting_queue[i].group_size;
    }
    for (int i = waiting_count; i < MAX_WAITING_GROUPS; i++) {
        shared_state->waiting_group_ids[i] = 0;
        shared_state->waiting_group_sizes[i] = 0;
    }
}

// Funkcja dodająca klienta do kolejki oczekujących
static int add_to_waiting_queue(int group_id, int group_size) {
    if (waiting_count >= MAX_WAITING) {
        return 0;
    }
    waiting_queue[waiting_count].group_id = group_id;
    waiting_queue[waiting_count].group_size = group_size;
    waiting_count++;
    sync_waiting_queue_to_shared();
    return 1;
}

// Funkcja próbująca obsłużyć klientów z kolejki oczekujących
static void try_serve_waiting_clients(void) {
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    while (waiting_count > 0) {
        int group_id = waiting_queue[0].group_id;
        int group_size = waiting_queue[0].group_size;
        
        int table_type, table_index;
        if (find_free_table(group_size, &table_type, &table_index)) {
            allocate_table(table_type, table_index, group_size, group_id);
            
            int group_idx = group_id % MAX_GROUPS;
            group_to_table_type[group_idx] = table_type;
            group_to_table_index[group_idx] = table_index;
            
            Message response;
            response.mtype = 1000 + group_id;
            response.group_id = group_id;
            response.group_size = group_size;
            response.table_type = table_type;
            response.table_index = table_index;
            
            if (msgsnd(msg_queue_id, &response, msg_size, 0) == -1) {
                log_message("OBSLUGA: Błąd wysyłania do klienta #%d z kolejki", group_id);
            } else {
                log_message("OBSLUGA: Klient #%d z kolejki -> stolik %d-os.[%d]", 
                           group_id, table_type, table_index);
            }
            
            for (int j = 0; j < waiting_count - 1; j++) {
                waiting_queue[j] = waiting_queue[j + 1];
            }
            waiting_count--;
            sync_waiting_queue_to_shared();
        } else {
            break;
        }
    }
}

// Funkcja obsługująca sygnały
static void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        sem_wait_op(sem_id, SEM_SHARED_STATE);
        
        if (shared_state->x3_doubled == 0) {
            shared_state->x3_doubled = 1;
            int old_x3 = shared_state->effective_x3;
            shared_state->effective_x3 = X3 * 2;
            int new_seats = X3 * 3;
            shared_state->total_free_seats += new_seats;
            
            log_message("OBSLUGA: X3 podwojone: %d -> %d stolików (+%d miejsc)", 
                       old_x3, shared_state->effective_x3, new_seats);
        } else {
            log_message("OBSLUGA: SYGNAŁ 1 (SIGUSR1) otrzymany ponownie - operacja NIEMOŻLIWA (stoliki 3-osobowe już zostały podwojone)");
        }
        
        sem_signal_op(sem_id, SEM_SHARED_STATE);
    } else if (sig == SIGUSR2) {
        // Rezerwacja obslugiwana przez MSG_TYPE_RESERVE_SEATS
    } else if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

int main(void) {
    log_message("OBSLUGA: Start pracy obsługi");
    
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    shared_state = get_shared_memory();
    if (shared_state == NULL) {
        handle_error("OBSLUGA: get_shared_memory failed");
    }
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("OBSLUGA: get_message_queue failed");
    }
    
    sem_id = get_semaphores();
    if (sem_id == -1) {
        handle_error("OBSLUGA: get_semaphores failed");
    }

    Message msg;
    ssize_t msg_size = sizeof(Message) - sizeof(long);

    for (int i = 0; i < MAX_GROUPS; i++) {
        group_to_table_type[i] = 0;
        group_to_table_index[i] = -1;
    }
    
    while (running) {
        ssize_t received = -1;
        
        received = msgrcv(msg_queue_id, &msg, msg_size, MSG_TYPE_SEAT_REQUEST, IPC_NOWAIT);
        
        if (received == -1 && (errno == ENOMSG || errno == EAGAIN)) {
            received = msgrcv(msg_queue_id, &msg, msg_size, MSG_TYPE_DISHES, IPC_NOWAIT);
        }
        
        if (received == -1 && (errno == ENOMSG || errno == EAGAIN)) {
            received = msgrcv(msg_queue_id, &msg, msg_size, MSG_TYPE_RESERVE_SEATS, IPC_NOWAIT);
        }
        
        if (received == -1) {
            if (errno == ENOMSG || errno == EAGAIN) {
                usleep(10000);
                continue;
            }
            if (errno == EINTR) {
                if (!running) break;
                continue;
            } else {
                if (!running) break;
                continue;
            }
        }
        
        if (msg.mtype == MSG_TYPE_SEAT_REQUEST) {
            sem_wait_op(sem_id, SEM_SHARED_STATE);
            
            int table_type, table_index;
            if (find_free_table(msg.group_size, &table_type, &table_index)) {
                allocate_table(table_type, table_index, msg.group_size, msg.group_id);
                
                int group_idx = msg.group_id % MAX_GROUPS;
                group_to_table_type[group_idx] = table_type;
                group_to_table_index[group_idx] = table_index;
                
                sem_signal_op(sem_id, SEM_SHARED_STATE);
                
                Message response;
                response.mtype = 1000 + msg.group_id;
                response.group_id = msg.group_id;
                response.group_size = msg.group_size;
                response.table_type = table_type;
                response.table_index = table_index;
                
                if (msgsnd(msg_queue_id, &response, msg_size, 0) == -1) {
                    log_message("OBSLUGA: Błąd wysyłania odpowiedzi do #%d", msg.group_id);
                }
                
                log_message("OBSLUGA: Stolik %d-os.[%d] -> grupa #%d", 
                           table_type, table_index, msg.group_id);
            } else {
                if (add_to_waiting_queue(msg.group_id, msg.group_size)) {
                    log_message("OBSLUGA: Grupa #%d (%d os.) czeka w kolejce (pozycja %d)", 
                               msg.group_id, msg.group_size, waiting_count);
                } else {
                    Message response;
                    response.mtype = 1000 + msg.group_id;
                    response.group_id = msg.group_id;
                    response.group_size = msg.group_size;
                    response.table_type = 0;
                    response.table_index = -1;
                    
                    msgsnd(msg_queue_id, &response, msg_size, 0);
                    log_message("OBSLUGA: Kolejka pełna - grupa #%d odrzucona", msg.group_id);
                }
                
                sem_signal_op(sem_id, SEM_SHARED_STATE);
            }
            
        } else if (msg.mtype == MSG_TYPE_DISHES) {
            sem_wait_op(sem_id, SEM_SHARED_STATE);
            
            int group_idx = msg.group_id % MAX_GROUPS;
            int table_type = group_to_table_type[group_idx];
            int table_index = group_to_table_index[group_idx];
            
            if (table_type > 0 && table_index >= 0) {
                free_table(table_type, table_index, msg.group_size, msg.group_id);
                shared_state->dirty_dishes += msg.group_size;
                
                log_message("OBSLUGA: Grupa #%d zwolniła stolik (naczynia: %d)", 
                           msg.group_id, shared_state->dirty_dishes);
                
                group_to_table_type[group_idx] = 0;
                group_to_table_index[group_idx] = -1;
                
                try_serve_waiting_clients();
            }
            
            sem_signal_op(sem_id, SEM_SHARED_STATE);
            
        } else if (msg.mtype == MSG_TYPE_RESERVE_SEATS) {
            int tables_to_reserve = msg.group_size;
            
            sem_wait_op(sem_id, SEM_SHARED_STATE);
            
            typedef struct {
                int type;
                int index;
                int seats;
            } TableInfo;
            
            TableInfo free_tables[100];
            int free_count = 0;
            
            for (int i = 0; i < X4; i++) {
                if (shared_state->table_4[i] == 0) {
                    free_tables[free_count].type = 4;
                    free_tables[free_count].index = i;
                    free_tables[free_count].seats = 4;
                    free_count++;
                }
            }
            
            int x3_limit = shared_state->effective_x3;
            for (int i = 0; i < x3_limit; i++) {
                if (shared_state->table_3[i] == 0) {
                    free_tables[free_count].type = 3;
                    free_tables[free_count].index = i;
                    free_tables[free_count].seats = 3;
                    free_count++;
                }
            }
            
            for (int i = 0; i < X2; i++) {
                if (shared_state->table_2[i] == 0) {
                    free_tables[free_count].type = 2;
                    free_tables[free_count].index = i;
                    free_tables[free_count].seats = 2;
                    free_count++;
                }
            }
            
            for (int i = 0; i < X1; i++) {
                if (shared_state->table_1[i] == 0) {
                    free_tables[free_count].type = 1;
                    free_tables[free_count].index = i;
                    free_tables[free_count].seats = 1;
                    free_count++;
                }
            }
            
            int tables_reserved = 0;
            int seats_reserved = 0;
            
            if (free_count > 0 && tables_to_reserve > 0) {
                int to_reserve = (tables_to_reserve < free_count) ? tables_to_reserve : free_count;
                
                for (int i = 0; i < to_reserve; i++) {
                    int j = i + (rand() % (free_count - i));
                    
                    TableInfo temp = free_tables[i];
                    free_tables[i] = free_tables[j];
                    free_tables[j] = temp;
                    
                    int type = free_tables[i].type;
                    int idx = free_tables[i].index;
                    int seats = free_tables[i].seats;
                    
                    switch (type) {
                        case 1: shared_state->table_1[idx] = -1; break;
                        case 2: shared_state->table_2[idx] = -1; break;
                        case 3: shared_state->table_3[idx] = -1; break;
                        case 4: shared_state->table_4[idx] = -1; break;
                    }
                    
                    tables_reserved++;
                    seats_reserved += seats;
                    shared_state->total_free_seats -= seats;
                    
                    log_message("OBSLUGA: Zarezerwowano stolik %d-os.[%d]", type, idx);
                }
            }
            
            shared_state->reserved_seats = seats_reserved;
            
            log_message("OBSLUGA: Rezerwacja kierownika: %d stolików (%d miejsc)", 
                       tables_reserved, seats_reserved);
            
            sem_signal_op(sem_id, SEM_SHARED_STATE);
        }
    }

    // Wyslij odpowiedzi do czekajacych w kolejce
    ssize_t final_msg_size = sizeof(Message) - sizeof(long);
    for (int i = 0; i < waiting_count; i++) {
        Message response;
        response.mtype = 1000 + waiting_queue[i].group_id;
        response.group_id = waiting_queue[i].group_id;
        response.group_size = waiting_queue[i].group_size;
        response.table_type = 0;
        response.table_index = -1;
        msgsnd(msg_queue_id, &response, final_msg_size, IPC_NOWAIT);
    }

    int is_fire = 0;
    if (shared_state != NULL) {
        is_fire = shared_state->fire_alarm;
    }
    
    if (is_fire) {
        log_message("OBSLUGA: Pracownicy kończą pracę (pożar)");
    } else {
        log_message("OBSLUGA: Pracownicy kończą pracę");
    }

    if (shared_state != NULL) {
        shmdt(shared_state);
    }
    
    return EXIT_SUCCESS;
}
