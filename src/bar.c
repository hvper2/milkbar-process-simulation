#include "common.h"
#include "utils.h"

static pid_t pid_kasjer = -1;
static pid_t pid_obsluga = -1;
static pid_t pid_kierownik = -1;
static int running = 1;

void signal_handler(int sig) {
    log_message("BAR: Otrzymano sygnał %d - rozpoczynam bezpieczne zakończenie", sig);
    running = 0;
    
    if (pid_kasjer > 0) {
        kill(pid_kasjer, SIGTERM);
    }
    if (pid_obsluga > 0) {
        kill(pid_obsluga, SIGTERM);
    }
    if (pid_kierownik > 0) {
        kill(pid_kierownik, SIGTERM);
    }
    
    sleep(1);
    cleanup_ipc();
    exit(EXIT_SUCCESS);
}

static pid_t clients_pgid = -1;  // PGID grupy klientów

pid_t spawn_process(const char *program_path, const char *program_name) {
    pid_t pid = fork();
    
    if (pid == -1) {
        handle_error("spawn_process: fork failed");
        return -1;
    } else if (pid == 0) {
        if (execl(program_path, program_name, (char *)NULL) == -1) {
            perror("spawn_process: execl failed");
            exit(EXIT_FAILURE);
        }
    } else {
        log_message("BAR: Uruchomiono proces %s (PID=%d)", program_name, pid);
        return pid;
    }
    
    return -1;
}

// Spawn klienta w specjalnej grupie procesów (do obsługi pożaru)
pid_t spawn_client(const char *program_path, const char *program_name) {
    pid_t pid = fork();
    
    if (pid == -1) {
        handle_error("spawn_client: fork failed");
        return -1;
    } else if (pid == 0) {
        // Proces potomny - dołącz do grupy klientów
        if (clients_pgid > 0) {
            setpgid(0, clients_pgid);
        }
        if (execl(program_path, program_name, (char *)NULL) == -1) {
            perror("spawn_client: execl failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // Proces macierzysty
        if (clients_pgid == -1) {
            // Pierwszy klient - stwórz nową grupę procesów
            clients_pgid = pid;
            setpgid(pid, pid);
            log_message("BAR: Utworzono grupę klientów (PGID=%d)", clients_pgid);
        } else {
            // Kolejni klienci - dołącz do istniejącej grupy
            setpgid(pid, clients_pgid);
        }
        log_message("BAR: Uruchomiono klienta %s (PID=%d, PGID=%d)", program_name, pid, clients_pgid);
        return pid;
    }
    
    return -1;
}

int main(void) {
    log_message("BAR: Rozpoczęcie symulacji baru mlecznego");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Tworzenie zasobów IPC
    log_message("BAR: Tworzenie zasobów IPC...");
    init_logger();
    create_shared_memory();
    create_message_queue();
    create_semaphores();
    log_message("BAR: Wszystkie zasoby IPC utworzone pomyślnie");
    
    // Uruchomienie procesów
    log_message("BAR: Uruchamianie procesów...");
    
    pid_kasjer = spawn_process("./bin/kasjer", "kasjer");
    if (pid_kasjer == -1) {
        log_message("BAR: Błąd uruchamiania kasjera");
        cleanup_ipc();
        return EXIT_FAILURE;
    }
    
    pid_obsluga = spawn_process("./bin/obsluga", "obsluga");
    if (pid_obsluga == -1) {
        log_message("BAR: Błąd uruchamiania obsługi");
        cleanup_ipc();
        return EXIT_FAILURE;
    }
    
    // Najpierw wygeneruj pierwszego klienta, żeby ustalić PGID
    log_message("BAR: Tworzenie pierwszego klienta dla ustalenia PGID...");
    pid_t first_client = spawn_client("./bin/klient", "klient");
    if (first_client == -1) {
        log_message("BAR: Błąd tworzenia pierwszego klienta");
    } else {
        log_message("BAR: Pierwszy klient utworzony (PID=%d), PGID klientów=%d", first_client, clients_pgid);
    }
    
    char pid_obsluga_str[32];
    char pid_kasjer_str[32];
    char clients_pgid_str[32];
    snprintf(pid_obsluga_str, sizeof(pid_obsluga_str), "%d", pid_obsluga);
    snprintf(pid_kasjer_str, sizeof(pid_kasjer_str), "%d", pid_kasjer);
    snprintf(clients_pgid_str, sizeof(clients_pgid_str), "%d", clients_pgid);
    
    pid_kierownik = fork();
    if (pid_kierownik == -1) {
        handle_error("BAR: fork kierownik failed");
    } else if (pid_kierownik == 0) {
        // Przekazujemy PGID klientów jako trzeci argument
        execl("./bin/kierownik", "kierownik", pid_obsluga_str, pid_kasjer_str, clients_pgid_str, (char *)NULL);
        perror("BAR: execl kierownik failed");
        exit(EXIT_FAILURE);
    } else {
        log_message("BAR: Uruchomiono proces kierownik (PID=%d) z PGID klientów=%d", pid_kierownik, clients_pgid);
    }
    
    log_message("BAR: Wszystkie procesy uruchomione pomyślnie");
    
    // Generator klientów
    log_message("BAR: Rozpoczęcie generowania klientów");
    
    time_t start_time = time(NULL);
    time_t current_time;
    int client_counter = 0;
    
    srand(time(NULL));
    
    while (running) {
        current_time = time(NULL);
        
        if (current_time - start_time >= SIMULATION_TIME) {
            log_message("BAR: Czas symulacji minął - kończę generowanie klientów");
            break;
        }
        
        sleep(CLIENT_INTERVAL);
        
        if (!running) {
            break;
        }
        
        client_counter++;
        pid_t pid_klient = spawn_client("./bin/klient", "klient");
        if (pid_klient == -1) {
            log_message("BAR: Błąd uruchamiania klienta #%d", client_counter);
        } else {
            log_message("BAR: Wygenerowano klienta #%d (PID=%d, PGID=%d)", client_counter, pid_klient, clients_pgid);
        }
    }
    
    log_message("BAR: Zakończono generowanie klientów (wygenerowano %d klientów)", client_counter);
    
    // Czekanie na zakończenie procesów
    log_message("BAR: Oczekiwanie na zakończenie procesów głównych...");
    int status;
    
    if (pid_kasjer > 0) {
        if (waitpid(pid_kasjer, &status, 0) == -1) {
            log_message("BAR: Błąd waitpid dla kasjera: %s", strerror(errno));
        } else {
            log_message("BAR: Kasjer zakończył pracę (status=%d)", status);
        }
    }
    
    if (pid_obsluga > 0) {
        if (waitpid(pid_obsluga, &status, 0) == -1) {
            log_message("BAR: Błąd waitpid dla obsługi: %s", strerror(errno));
        } else {
            log_message("BAR: Obsługa zakończyła pracę (status=%d)", status);
        }
    }
    
    if (pid_kierownik > 0) {
        if (waitpid(pid_kierownik, &status, 0) == -1) {
            log_message("BAR: Błąd waitpid dla kierownika: %s", strerror(errno));
        } else {
            log_message("BAR: Kierownik zakończył pracę (status=%d)", status);
        }
    }
    
    // Czekaj na wszystkich klientów przed usunięciem IPC
    log_message("BAR: Czekam na zakończenie wszystkich klientów...");
    int clients_finished = 0;
    pid_t child_pid;
    
    while ((child_pid = waitpid(-1, &status, 0)) > 0) {
        clients_finished++;
    }
    
    log_message("BAR: Wszyscy klienci zakończyli działanie (zakończono: %d procesów)", clients_finished);
    log_message("BAR: Wszystkie procesy zakończone");
    
    // Sprzątanie zasobów IPC
    log_message("BAR: Sprzątanie zasobów IPC...");
    cleanup_ipc();
    
    log_message("BAR: Symulacja zakończona pomyślnie");
    
    return EXIT_SUCCESS;
}
