#include "common.h"
#include "utils.h"

static pid_t pid_kasjer = -1;
static pid_t pid_obsluga = -1;
static pid_t pid_kierownik = -1;
static int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
    
    if (pid_kasjer > 0) kill(pid_kasjer, SIGTERM);
    if (pid_obsluga > 0) kill(pid_obsluga, SIGTERM);
    if (pid_kierownik > 0) kill(pid_kierownik, SIGTERM);
    
    sleep(1);
    cleanup_ipc();
    exit(EXIT_SUCCESS);
}

static pid_t clients_pgid = -1;  // PGID grupy klientów

pid_t spawn_process(const char *program_path, const char *program_name) {
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("spawn_process: fork failed");
        return -1;
    } else if (pid == 0) {
        if (execl(program_path, program_name, (char *)NULL) == -1) {
            perror("spawn_process: execl failed");
            exit(EXIT_FAILURE);
        }
    }
    return pid;
}

// Spawn klienta w specjalnej grupie procesów (do obsługi pożaru)
pid_t spawn_client(const char *program_path, const char *program_name) {
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("spawn_client: fork failed");
        return -1;
    } else if (pid == 0) {
        if (clients_pgid > 0) {
            setpgid(0, clients_pgid);
        }
        if (execl(program_path, program_name, (char *)NULL) == -1) {
            perror("spawn_client: execl failed");
            exit(EXIT_FAILURE);
        }
    } else {
        if (clients_pgid == -1) {
            clients_pgid = pid;
            setpgid(pid, pid);
        } else {
            setpgid(pid, clients_pgid);
        }
        return pid;
    }
    return -1;
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Tworzenie zasobów IPC
    init_logger();
    create_shared_memory();
    create_message_queue();
    create_semaphores();
    
    // Uruchomienie procesów
    pid_kasjer = spawn_process("./bin/kasjer", "kasjer");
    if (pid_kasjer == -1) {
        cleanup_ipc();
        return EXIT_FAILURE;
    }
    
    pid_obsluga = spawn_process("./bin/obsluga", "obsluga");
    if (pid_obsluga == -1) {
        cleanup_ipc();
        return EXIT_FAILURE;
    }
    
    // Pierwszy klient ustala PGID grupy
    pid_t first_client = spawn_client("./bin/klient", "klient");
    if (first_client == -1) {
        log_message("BAR: Błąd tworzenia pierwszego klienta");
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
        execl("./bin/kierownik", "kierownik", pid_obsluga_str, pid_kasjer_str, clients_pgid_str, (char *)NULL);
        perror("BAR: execl kierownik failed");
        exit(EXIT_FAILURE);
    }
    
    log_message("BAR: Procesy uruchomione (kasjer, obsługa, kierownik)");
    
    // Generator klientów
    time_t start_time = time(NULL);
    time_t current_time;
    int client_counter = 0;
    
    srand(time(NULL));
    
    while (running) {
        current_time = time(NULL);
        
        if (current_time - start_time >= SIMULATION_TIME) {
            break;
        }
        
        sleep(CLIENT_INTERVAL);
        
        if (!running) {
            break;
        }
        
        client_counter++;
        spawn_client("./bin/klient", "klient");
    }
    
    log_message("BAR: Wygenerowano %d klientów w ciągu %ds", client_counter, SIMULATION_TIME);
    
    // Czekanie na zakończenie procesów
    int status;
    
    if (pid_kasjer > 0) waitpid(pid_kasjer, &status, 0);
    if (pid_obsluga > 0) waitpid(pid_obsluga, &status, 0);
    if (pid_kierownik > 0) waitpid(pid_kierownik, &status, 0);
    
    // Czekaj na wszystkich klientów
    while (waitpid(-1, &status, 0) > 0) { }
    
    // Sprzątanie zasobów IPC
    cleanup_ipc();

    return EXIT_SUCCESS;
}
