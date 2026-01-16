#include "common.h"
#include "utils.h"

static pid_t pid_obsluga = -1;
static pid_t pid_kasjer = -1;
static pid_t clients_pgid = -1;
static int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    if (argc > 1) pid_obsluga = atoi(argv[1]);
    if (argc > 2) pid_kasjer = atoi(argv[2]);
    if (argc > 3) clients_pgid = atoi(argv[3]);
    
    srand(time(NULL) ^ getpid());
    
    time_t start_time = time(NULL);
    int sigusr1_sent = 0;
    int sigusr2_sent = 0;
    int fire_sent = 0;
    
    while (running) {
        time_t current_time = time(NULL);
        int elapsed = (int)(current_time - start_time);
        
        if (SIGNAL1_TIME > 0 && !sigusr1_sent && elapsed >= SIGNAL1_TIME) {
            if (pid_obsluga > 0) {
                log_message("KIEROWNIK: >>> SYGNAŁ 1 (SIGUSR1) - podwojenie stolików 3-osobowych");
                kill(pid_obsluga, SIGUSR1);
                sigusr1_sent = 1;
            }
        }
        
        if (SIGNAL2_TIME > 0 && !sigusr2_sent && elapsed >= SIGNAL2_TIME && elapsed < SIGNAL2_TIME + 1) {
            if (pid_obsluga > 0) {
                int tables_to_reserve = RESERVED_TABLE_COUNT;
                
                log_message("KIEROWNIK: >>> SYGNAŁ 2 (SIGUSR2) - rezerwacja %d stolików", tables_to_reserve);
                
                int msg_id = msgget(MSG_KEY, 0);
                if (msg_id != -1) {
                    Message reserve_msg;
                    reserve_msg.mtype = MSG_TYPE_RESERVE_SEATS;
                    reserve_msg.group_id = getpid();
                    reserve_msg.group_size = tables_to_reserve;
                    reserve_msg.table_type = 0;
                    reserve_msg.table_index = 0;
                    
                    ssize_t msg_size = sizeof(Message) - sizeof(long);
                    msgsnd(msg_id, &reserve_msg, msg_size, 0);
                }
                kill(pid_obsluga, SIGUSR2);
                sigusr2_sent = 1;
            }
        }
        
        if (SIGNAL3_TIME > 0 && !fire_sent && elapsed >= SIGNAL3_TIME) {
            fire_sent = 1;
            log_message("KIEROWNIK: >>> SYGNAŁ 3 (POŻAR) - ewakuacja wszystkich klientów");
            
            int shm_id = shmget(SHM_KEY, sizeof(SharedState), 0);
            if (shm_id != -1) {
                SharedState *state = (SharedState *)shmat(shm_id, NULL, 0);
                if (state != (void *)-1) {
                    state->fire_alarm = 1;
                    shmdt(state);
                }
            }
            
            kill(getppid(), SIGUSR1);
            
            if (clients_pgid > 0) {
                killpg(clients_pgid, SIGTERM);
            }
            
            sleep(2);
            
            if (pid_obsluga > 0) {
                kill(pid_obsluga, SIGTERM);
            }
            if (pid_kasjer > 0) {
                kill(pid_kasjer, SIGTERM);
            }
            
            break;
        }
        
        if (elapsed >= SIMULATION_TIME) {
            break;
        }
        
        sleep(1);
    }
    
    if (!fire_sent) {
        log_message("KIEROWNIK: Koniec symulacji - zamykanie baru");
        
        kill(getppid(), SIGUSR1);
        
        sleep(2);
        
        if (clients_pgid > 0) {
            killpg(clients_pgid, SIGTERM);
        }
        
        sleep(1);
        
        if (pid_obsluga > 0) {
            kill(pid_obsluga, SIGTERM);
        }
        if (pid_kasjer > 0) {
            kill(pid_kasjer, SIGTERM);
        }
        
        log_message("KIEROWNIK: Bar zamknięty");
    }
    
    return EXIT_SUCCESS;
}
