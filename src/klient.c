#include "common.h"
#include "utils.h"

static int msg_queue_id = -1;
static int group_id = 0;
static int group_size = 0;
static int is_parent = 1;
static int running = 1;
static int can_start_eating = 0;
static int can_exit = 0;
static pid_t *child_pids = NULL;

// Zabija procesy potomne i zwalnia pamięć child_pids.
static void cleanup_children(int send_signal) {
    if (group_size > 1 && child_pids != NULL && send_signal > 0) {
        for (int i = 0; i < group_size - 1; i++) {
            kill(child_pids[i], send_signal);
        }
    }
    if (child_pids != NULL) {
        free(child_pids);
        child_pids = NULL;
    }
}

// Sprawdza, czy alarm pożaru jest aktywny
static int check_fire_alarm(void) {
    int shm_id = shmget(SHM_KEY, sizeof(SharedState), 0);
    if (shm_id != -1) {
        SharedState *state = (SharedState *)shmat(shm_id, NULL, 0);
        if (state != (void *)-1) {
            int is_fire = state->fire_alarm;
            shmdt(state);
            return is_fire;
        }
    }
    return 0;
}

// Loguje ewakuację klienta w przypadku pożaru
static void log_evacuation(int gid) {
    if (check_fire_alarm()) {
        log_message("KLIENT #%d: POŻAR! Przerwano jedzenie - ewakuacja", gid);
    }
}

// Obsługa sygnałów
void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        can_start_eating = 1;
    } else if (sig == SIGUSR2) {
        can_exit = 1;
    } else {
        running = 0;
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc > 1) {
        group_size = atoi(argv[1]);
        if (group_size < 1 || group_size > 3) {
            group_size = (rand() % 3) + 1;
        }
    } else {
        group_size = (rand() % 3) + 1;
    }
    
    group_id = getpid();
    is_parent = 1;
    
    log_message("KLIENT #%d: Grupa %d-osobowa wchodzi do baru", group_id, group_size);
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("KLIENT: get_message_queue failed");
    }
    
    if (group_size > 1 && is_parent) {
        child_pids = malloc((group_size - 1) * sizeof(pid_t));
        if (child_pids == NULL) {
            handle_error("KLIENT: malloc failed");
        }
        
        for (int i = 0; i < group_size - 1; i++) {
            pid_t child_pid = fork();
            
            if (child_pid == -1) {
                perror("KLIENT: fork failed");
                for (int j = 0; j < i; j++) {
                    kill(child_pids[j], SIGTERM);
                }
                cleanup_children(0);
                return EXIT_FAILURE;
            } else if (child_pid == 0) {
                is_parent = 0;
                group_id = getppid();
                break;
            } else {
                child_pids[i] = child_pid;
            }
        }
    }
    
    int orders = 1;
    if (is_parent) {
        orders = (rand() % 100) < NO_ORDER_PROBABILITY ? 0 : 1;
        
        if (!orders) {
            log_message("KLIENT #%d: Nie zamawia - wychodzi (5%% przypadek)", group_id);
            cleanup_children(SIGTERM);
            return EXIT_SUCCESS;
        }
    }
    
    if (is_parent) {
        Message seat_request;
        seat_request.mtype = MSG_TYPE_SEAT_REQUEST;
        seat_request.group_id = group_id;
        seat_request.group_size = group_size;
        seat_request.table_type = 0;
        seat_request.table_index = 0;
        
        ssize_t msg_size = sizeof(Message) - sizeof(long);
        
        if (msgsnd(msg_queue_id, &seat_request, msg_size, 0) == -1) {
            perror("KLIENT: msgsnd (rezerwacja) failed");
            cleanup_children(SIGTERM);
            return EXIT_FAILURE;
        }
        
        Message seat_response;
        long reply_type = 1000 + group_id;
        
        ssize_t received = msgrcv(msg_queue_id, &seat_response, msg_size, reply_type, 0);
        if (received == -1) {
            if (errno == EINTR && !running) {
                log_evacuation(group_id);
                cleanup_children(SIGTERM);
                return EXIT_SUCCESS;
            }
            log_message("KLIENT #%d: Błąd msgrcv (oczekiwanie na stolik): %s", group_id, strerror(errno));
            cleanup_children(SIGTERM);
            return EXIT_FAILURE;
        }
        
        if (seat_response.table_type == 0 || seat_response.table_index < 0) {
            log_message("KLIENT #%d: Brak wolnych miejsc - wychodzi BEZ odbierania dania", group_id);
            cleanup_children(SIGTERM);
            return EXIT_SUCCESS;
        }
        
        log_message("KLIENT #%d: Stolik %d-os. zarezerwowany -> płaci", 
                   group_id, seat_response.table_type);
        
        Message payment_msg;
        payment_msg.mtype = MSG_TYPE_PAYMENT;
        payment_msg.group_id = group_id;
        payment_msg.group_size = group_size;
        payment_msg.table_type = seat_response.table_type;
        payment_msg.table_index = seat_response.table_index;
        
        if (msgsnd(msg_queue_id, &payment_msg, msg_size, 0) == -1) {
            if (!running) {
                cleanup_children(0);
                return EXIT_SUCCESS;
            }
            perror("KLIENT: msgsnd (płatność) failed");
            cleanup_children(0);
            return EXIT_FAILURE;
        }
        
        Message payment_response;
        long payment_reply_type = 2000 + group_id;
        
        ssize_t payment_received = msgrcv(msg_queue_id, &payment_response, msg_size, payment_reply_type, 0);
        if (payment_received == -1) {
            if (errno == EINTR && !running) {
                log_evacuation(group_id);
                cleanup_children(0);
                return EXIT_SUCCESS;
            }
            log_message("KLIENT #%d: Błąd oczekiwania na potwierdzenie płatności", group_id);
            cleanup_children(0);
            return EXIT_FAILURE;
        }
        
        log_message("KLIENT #%d: Płatność przyjęta -> odbiera danie", group_id);
        
        if (!running) {
            log_evacuation(group_id);
            cleanup_children(0);
            return EXIT_SUCCESS;
        }
        
        if (group_size > 1 && child_pids != NULL) {
            for (int i = 0; i < group_size - 1; i++) {
                kill(child_pids[i], SIGUSR1);
            }
        }
        
        log_message("KLIENT #%d: Rozpoczyna jedzenie (czas: %ds)", group_id, EATING_TIME);
    } else {
        while (!can_start_eating && running) {
            pause();
            if (!running) {
                return EXIT_SUCCESS;
            }
        }
        
        if (!running) {
            return EXIT_SUCCESS;
        }
    }
    
    sleep(1);
    
    if (!running) {
        if (is_parent) {
            log_evacuation(group_id);
            cleanup_children(0);
        }
        return EXIT_SUCCESS;
    }
    
    sleep(EATING_TIME);
    
    if (!running) {
        if (is_parent) {
            log_evacuation(group_id);
            cleanup_children(0);
        }
        return EXIT_SUCCESS;
    }
    
    if (is_parent) {
        log_message("KLIENT #%d: Skończył jeść", group_id);
        
        if (group_size > 1 && child_pids != NULL) {
            for (int i = 0; i < group_size - 1; i++) {
                kill(child_pids[i], SIGUSR2);
            }
            
            int status;
            for (int i = 0; i < group_size - 1; i++) {
                waitpid(child_pids[i], &status, 0);
            }
        }
        
        Message dishes_msg;
        dishes_msg.mtype = MSG_TYPE_DISHES;
        dishes_msg.group_id = group_id;
        dishes_msg.group_size = group_size;
        dishes_msg.table_type = 0;
        dishes_msg.table_index = -1;
        
        ssize_t msg_size = sizeof(Message) - sizeof(long);
        if (msgsnd(msg_queue_id, &dishes_msg, msg_size, 0) == -1) {
            if (!running) {
                cleanup_children(0);
                return EXIT_SUCCESS;
            }
            perror("KLIENT: msgsnd (naczynia) failed");
            cleanup_children(0);
            return EXIT_FAILURE;
        }
        
        log_message("KLIENT #%d: Oddał naczynia (%d szt.) i wychodzi z baru", group_id, group_size);
        cleanup_children(0);
    } else {
        while (!can_exit && running) {
            pause();
        }
    }
    
    return EXIT_SUCCESS;
}
