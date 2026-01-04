#include "common.h"
#include "utils.h"

static pid_t pid_obsluga = -1;
static pid_t pid_kasjer = -1;
static int running = 1;

void signal_handler(int sig) {
    log_message("KIEROWNIK: Otrzymano sygnał %d - kończę pracę", sig);
    running = 0;
}

int main(int argc, char *argv[]) {
    log_message("KIEROWNIK: Start pracy kierownika");
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    if (argc > 1) {
        pid_obsluga = atoi(argv[1]);
    }
    if (argc > 2) {
        pid_kasjer = atoi(argv[2]);
    }
    
    log_message("KIEROWNIK: PID obsługi=%d, PID kasjera=%d", pid_obsluga, pid_kasjer);
    
    time_t start_time = time(NULL);
    int sigusr1_sent = 0;
    
    log_message("KIEROWNIK: Rozpoczęcie zarządzania sygnałami");
    
    while (running) {
        time_t current_time = time(NULL);
        int elapsed = (int)(current_time - start_time);
        
        if (elapsed >= SIMULATION_TIME) {
            log_message("KIEROWNIK: Czas symulacji minął - wysyłam sygnał pożaru");
            break;
        }
        
        // SIGUSR1 - podwój X3 (po 10s, tylko raz)
        
        
        // SIGUSR2 - rezerwacja (po 15s)
        
        
        sleep(1);
    }
    
    // Sygnał pożaru - SIGTERM
    
    
    return EXIT_SUCCESS;
}
