#include "common.h"
#include "utils.h"

static pid_t pid_obsluga = -1;
static pid_t pid_kasjer = -1;
static pid_t clients_pgid = -1;  // PGID grupy klientów
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
    if (argc > 3) {
        clients_pgid = atoi(argv[3]);
    }
    
    log_message("KIEROWNIK: PID obsługi=%d, PID kasjera=%d, PGID klientów=%d", pid_obsluga, pid_kasjer, clients_pgid);
    
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
        if (!sigusr1_sent && elapsed >= 10) {
            if (pid_obsluga > 0) {
                log_message("KIEROWNIK: Wysyłam SIGUSR1 do obsługi (podwój X3)");
                if (kill(pid_obsluga, SIGUSR1) == -1) {
                    log_message("KIEROWNIK: Błąd kill(SIGUSR1): %s", strerror(errno));
                }
                sigusr1_sent = 1;
            }
        }
        
        // SIGUSR2 - rezerwacja (po 15s)
        if (elapsed >= 15 && elapsed < 16) {
            if (pid_obsluga > 0) {
                log_message("KIEROWNIK: Wysyłam SIGUSR2 do obsługi (rezerwacja miejsc)");
                if (kill(pid_obsluga, SIGUSR2) == -1) {
                    log_message("KIEROWNIK: Błąd kill(SIGUSR2): %s", strerror(errno));
                }
            }
        }
        
        sleep(1);
    }
    
    // Sygnał pożaru - SIGTERM
    log_message("KIEROWNIK: POŻAR - Wysyłam sygnał do wszystkich procesów");
    
    // sygnał do klientow
    if (clients_pgid > 0) {
        log_message("KIEROWNIK: Wysyłam SIGTERM do WSZYSTKICH klientów (PGID=%d)", clients_pgid);
        if (killpg(clients_pgid, SIGTERM) == -1) {
            log_message("KIEROWNIK: Błąd killpg(SIGTERM do klientów): %s", strerror(errno));
        } else {
            log_message("KIEROWNIK: Sygnał pożaru wysłany do grupy klientów");
        }
    }
    
    sleep(1);
    
    // sygnał do obsługi i kasjera
    log_message("KIEROWNIK: Klienci ewakuowani - kończę pracę obsługi i kasy");
    
    if (pid_obsluga > 0) {
        if (kill(pid_obsluga, SIGTERM) == -1) {
            log_message("KIEROWNIK: Błąd kill(SIGTERM do obsługi): %s", strerror(errno));
        }
    }
    
    if (pid_kasjer > 0) {
        if (kill(pid_kasjer, SIGTERM) == -1) {
            log_message("KIEROWNIK: Błąd kill(SIGTERM do kasjera): %s", strerror(errno));
        }
    }
    
    log_message("KIEROWNIK: Sygnały pożaru wysłane - kończę pracę");
    
    return EXIT_SUCCESS;
}
