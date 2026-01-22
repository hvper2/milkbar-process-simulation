#include "common.h"
#include "utils.h"

static pid_t pid_kasjer = -1;
static pid_t pid_obsluga = -1;
static pid_t pid_kierownik = -1;
static int running = 1; 
static pid_t clients_pgid = -1; 

static pid_t client_pids[TOTAL_CLIENTS];
static int num_clients = 0;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
    }
}

static pid_t spawn_process(const char *program_path, const char *program_name) {
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

static pid_t spawn_client(const char *program_path, const char *program_name, const char *group_size_str) {
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("spawn_client: fork failed");
        return -1;
    } else if (pid == 0) {
        if (clients_pgid > 0) {
            setpgid(0, clients_pgid);
        }
        if (execl(program_path, program_name, group_size_str, (char *)NULL) == -1) {
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
    }
    return pid;
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGCHLD, sigchld_handler);
    
    srand(time(NULL));
    
    init_logger();
    create_shared_memory();
    create_message_queue();
    create_semaphores();
    
    // Ustawienie czasu startu symulacji w pamięci dzielonej
    SharedState *shared_state = get_shared_memory();
    if (shared_state != NULL) {
        shared_state->simulation_start_time = time(NULL);
    }
    
    log_message("BAR: Inicjalizacja zakończona, uruchamiam pracowników...");
    
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

    usleep(50000);
    
    char pid_obsluga_str[32];
    char pid_kasjer_str[32];
    char clients_pgid_str[32];
    snprintf(pid_obsluga_str, sizeof(pid_obsluga_str), "%d", pid_obsluga);
    snprintf(pid_kasjer_str, sizeof(pid_kasjer_str), "%d", pid_kasjer);
    snprintf(clients_pgid_str, sizeof(clients_pgid_str), "%d", -1);  
    
    pid_kierownik = fork();
    if (pid_kierownik == -1) {
        handle_error("BAR: fork kierownik failed");
    } else if (pid_kierownik == 0) {
        execl("./bin/kierownik", "kierownik", pid_obsluga_str, pid_kasjer_str, clients_pgid_str, (char *)NULL);
        perror("BAR: execl kierownik failed");
        exit(EXIT_FAILURE);
    }
    
    log_message("BAR: Procesy uruchomione (kasjer, obsługa, kierownik)");
    
    log_message("BAR: Generuję %d grup klientów...", TOTAL_CLIENTS);
    
    for (int i = 0; i < TOTAL_CLIENTS && running; i++) {
        int group_size = (rand() % 3) + 1;
        char group_size_str[8];
        snprintf(group_size_str, sizeof(group_size_str), "%d", group_size);
        
        pid_t client_pid = spawn_client("./bin/klient", "klient", group_size_str);
        if (client_pid > 0) {
            client_pids[num_clients++] = client_pid;
        }
        
        usleep(500000);
        // sleep(1);  
    }
    
    log_message("BAR: Wygenerowano %d grup klientów", num_clients);
    
    if (shared_state != NULL && clients_pgid > 0) {
        shared_state->clients_pgid = clients_pgid;
    }
    
    if (shared_state == NULL) {
        shared_state = get_shared_memory();
        if (shared_state == NULL) {
            cleanup_ipc();
            return EXIT_FAILURE;
        }
    }
    
    time_t start_time = shared_state->simulation_start_time;
    
    while (running) {
        time_t current_time = time(NULL);
        
        if (current_time - start_time >= SIMULATION_TIME) {
            log_message("BAR: Koniec czasu symulacji");
            break;
        }
        
        if (shared_state->fire_alarm) {
            log_message("BAR: Pożar! Kończę symulację.");
            break;
        }
        
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {
        }
        
        sleep(2);
    }
    
    if (shared_state != NULL) {
        shmdt(shared_state);
    }
    
    int status;
    
    if (pid_kasjer > 0) waitpid(pid_kasjer, &status, 0);
    if (pid_obsluga > 0) waitpid(pid_obsluga, &status, 0);
    if (pid_kierownik > 0) waitpid(pid_kierownik, &status, 0);
    
    while (waitpid(-1, &status, 0) > 0) { }
    
    log_message("BAR: Symulacja zakończona");
    
    cleanup_ipc();

    return EXIT_SUCCESS;
}
