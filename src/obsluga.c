#include "common.h"
#include "utils.h"

static SharedState *shared_state = NULL;
static int shm_id = -1;
static int msg_queue_id = -1;
static int sem_id = -1;
static int running = 1;

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
    // Stoliki 1-osobowe - tylko dla grup 1-osobowych
    if (group_size == 1) {
        for (int i = 0; i < X1; i++) {
            if (shared_state->table_1[i] == 0) {
                *table_type = 1;
                *table_index = i;
                return 1;
            }
        }
    }
    
    // Stoliki 2-osobowe - grupy 1-2 os., współdzielenie dla równolicznych
    if (group_size <= 2) {
        for (int i = 0; i < X2; i++) {
            int occupied = shared_state->table_2[i];
            
            // Wolny stolik
            if (occupied == 0) {
                *table_type = 2;
                *table_index = i;
                return 1;
            }
            // Współdzielenie: dwie grupy 1-os. mogą siedzieć przy stoliku 2-os.
            if (group_size == 1 && occupied == 1 && occupied + group_size <= 2) {
                *table_type = 2;
                *table_index = i;
                return 1;
            }
        }
    }
    
    // Stoliki 3-osobowe - używamy effective_x3, współdzielenie dla równolicznych
    int x3_limit = shared_state->effective_x3;
    
    if (group_size <= 3) {
        for (int i = 0; i < x3_limit; i++) {
            int occupied = shared_state->table_3[i];
            
            // Wolny stolik
            if (occupied == 0) {
                *table_type = 3;
                *table_index = i;
                return 1;
            }
            // Współdzielenie: równoliczne grupy mogą dzielić stolik (np. 1+1, 1+1+1)
            if (occupied == group_size && occupied + group_size <= 3) {
                *table_type = 3;
                *table_index = i;
                return 1;
            }
        }
    }
    
    // Stoliki 4-osobowe - grupy 1-4 os., współdzielenie dla równolicznych
    if (group_size <= 4) {
        for (int i = 0; i < X4; i++) {
            int occupied = shared_state->table_4[i];
            
            if (occupied == 0) {
                *table_type = 4;
                *table_index = i;
                return 1;
            }
            // Współdzielenie: równoliczne grupy (1+1, 1+1+1, 1+1+1+1, 2+2)
            if (occupied == group_size && occupied + group_size <= 4) {
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
            // Współdzielenie stolików 2-os. - dodajemy do istniejącej wartości
            shared_state->table_2[table_index] += group_size;
            break;
        case 3:
            // Współdzielenie stolików 3-os. - dodajemy do istniejącej wartości
            shared_state->table_3[table_index] += group_size;
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
            // Współdzielenie - odejmujemy tylko swoją część
            shared_state->table_2[table_index] -= group_size;
            if (shared_state->table_2[table_index] < 0) {
                shared_state->table_2[table_index] = 0;
            }
            break;
        case 3:
            // Współdzielenie - odejmujemy tylko swoją część
            shared_state->table_3[table_index] -= group_size;
            if (shared_state->table_3[table_index] < 0) {
                shared_state->table_3[table_index] = 0;
            }
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
        sem_wait(sem_id, SEM_SHARED_STATE);
        
        if (shared_state->x3_doubled == 0) {
            shared_state->x3_doubled = 1;
            int old_x3 = shared_state->effective_x3;
            shared_state->effective_x3 = X3 * 2;
            int new_seats = X3 * 3;
            shared_state->total_free_seats += new_seats;
            
            log_message("OBSLUGA: X3 podwojone: %d -> %d stolików (+%d miejsc)", 
                       old_x3, shared_state->effective_x3, new_seats);
        }
        
        sem_signal(sem_id, SEM_SHARED_STATE);
        
    } else if (sig == SIGUSR2) {
        // Liczba miejsc przychodzi przez komunikat
        
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

    Message msg;
    ssize_t msg_size = sizeof(Message) - sizeof(long);

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
                if (!running) {
                    break;
                }
                continue;
            }
        }
        
        // rezerwacja stolika
        if (msg.mtype == MSG_TYPE_SEAT_REQUEST) {
            sem_wait(sem_id, SEM_SHARED_STATE);
            
            Message response;
            response.group_id = msg.group_id;
            response.group_size = msg.group_size;
            
            int table_type, table_index;
            if (find_free_table(msg.group_size, &table_type, &table_index)) {
                // ATOMOWO rezerwujemy stolik. klient ma gwarancję miejsca
                allocate_table(table_type, table_index, msg.group_size);
                
                int group_idx = msg.group_id % MAX_GROUPS;
                group_to_table_type[group_idx] = table_type;
                group_to_table_index[group_idx] = table_index;
                
                response.mtype = MSG_TYPE_SEAT_CONFIRM;
                response.table_type = table_type;
                response.table_index = table_index;
                
                log_message("OBSLUGA: Stolik %d-os.[%d] -> grupa #%d", 
                           table_type, table_index, msg.group_id);
            } else {
                response.mtype = MSG_TYPE_SEAT_REJECT;
                response.table_type = 0;
                response.table_index = -1;
                
                log_message("OBSLUGA: BRAK MIEJSCA dla grupy #%d", msg.group_id);
            }
            
            sem_signal(sem_id, SEM_SHARED_STATE);
            
            // Wyślij odpowiedź do klienta (używamy group_id jako typu wiadomości)
            response.mtype = (msg.mtype == MSG_TYPE_SEAT_REQUEST) ? 
                             (response.table_type > 0 ? MSG_TYPE_SEAT_CONFIRM : MSG_TYPE_SEAT_REJECT) : response.mtype;
            // Używamy group_id + 1000 jako unikalnego typu dla odpowiedzi
            long reply_type = 1000 + msg.group_id;
            response.mtype = reply_type;
            
            if (msgsnd(msg_queue_id, &response, msg_size, 0) == -1) {
                log_message("OBSLUGA: Błąd wysyłania odpowiedzi do klienta #%d: %s", 
                           msg.group_id, strerror(errno));
            }
            
        } else if (msg.mtype == MSG_TYPE_PAID) {
            // Stolik już zarezerwowany - tylko potwierdzenie
            
        } else if (msg.mtype == MSG_TYPE_DISHES) {
            sem_wait(sem_id, SEM_SHARED_STATE);
            
            int group_idx = msg.group_id % MAX_GROUPS;
            int table_type = group_to_table_type[group_idx];
            int table_index = group_to_table_index[group_idx];
            
            if (table_type > 0 && table_index >= 0) {
                free_table(table_type, table_index, msg.group_size);
                shared_state->dirty_dishes += msg.group_size;
                
                log_message("OBSLUGA: Grupa #%d zwolniła stolik (naczynia: %d)", 
                           msg.group_id, shared_state->dirty_dishes);
                
                group_to_table_type[group_idx] = 0;
                group_to_table_index[group_idx] = -1;
            }
            
            sem_signal(sem_id, SEM_SHARED_STATE);
            
        } else if (msg.mtype == MSG_TYPE_RESERVE_SEATS) {
            int seats_to_reserve = msg.group_size;
            
            sem_wait(sem_id, SEM_SHARED_STATE);
            
            int available = shared_state->total_free_seats - shared_state->reserved_seats;
            if (seats_to_reserve <= available) {
                shared_state->reserved_seats += seats_to_reserve;
                log_message("OBSLUGA: Rezerwacja kierownika: %d miejsc (zarezerwowanych: %d)", 
                           seats_to_reserve, shared_state->reserved_seats);
            }
            
            sem_signal(sem_id, SEM_SHARED_STATE);
        }
    }

    if (shared_state != NULL) {
        shmdt(shared_state);
    }
    
    return EXIT_SUCCESS;
}
