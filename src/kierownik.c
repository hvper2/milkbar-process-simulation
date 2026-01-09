#include "common.h"
#include "utils.h"

static pid_t pid_obsluga = -1;
static pid_t pid_kasjer = -1;
static pid_t clients_pgid = -1;  // PGID grupy klientów
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
    
    while (running) {
        time_t current_time = time(NULL);
        int elapsed = (int)(current_time - start_time);
        
        if (elapsed >= SIMULATION_TIME) {
            break;
        }
        
        // SIGUSR1 - podwój X3 (po 10s, tylko raz)
        if (!sigusr1_sent && elapsed >= 10) {
            if (pid_obsluga > 0) {
                log_message("KIEROWNIK: >>> SIGUSR1 - podwojenie stolików 3-osobowych");
                kill(pid_obsluga, SIGUSR1);
                sigusr1_sent = 1;
            }
        }
        
        // SIGUSR2 - rezerwacja (po 15s) - z uzgodnieniem liczby miejsc
        if (elapsed >= 15 && elapsed < 16) {
            if (pid_obsluga > 0) {
                int seats_to_reserve = (rand() % 4) + 2;
                
                log_message("KIEROWNIK: >>> SIGUSR2 - rezerwacja %d miejsc", seats_to_reserve);
                
                int msg_id = msgget(MSG_KEY, 0);
                if (msg_id != -1) {
                    Message reserve_msg;
                    reserve_msg.mtype = MSG_TYPE_RESERVE_SEATS;
                    reserve_msg.group_id = getpid();
                    reserve_msg.group_size = seats_to_reserve;
                    reserve_msg.table_type = 0;
                    reserve_msg.table_index = 0;
                    
                    ssize_t msg_size = sizeof(Message) - sizeof(long);
                    msgsnd(msg_id, &reserve_msg, msg_size, 0);
                }
                kill(pid_obsluga, SIGUSR2);
            }
        }
        
        sleep(1);
    }
    
    // Sygnał pożaru - SIGTERM
    log_message("KIEROWNIK: >>> POŻAR! Ewakuacja wszystkich klientów");
    
    if (clients_pgid > 0) {
        killpg(clients_pgid, SIGTERM);
    }
    
    sleep(1);
    
    // sygnał do obsługi i kasjera
    if (pid_obsluga > 0) kill(pid_obsluga, SIGTERM);
    if (pid_kasjer > 0) kill(pid_kasjer, SIGTERM);
    
    return EXIT_SUCCESS;
}
