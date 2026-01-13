#include "common.h"
#include "utils.h"
#include <stdarg.h>

static int log_fd = -1;
static int log_sem_id = -1;
static int shm_id = -1;
static int msg_id = -1;
static int sem_id = -1;

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void init_logger(void) {
    const char *log_file = "logs/symulacja.log";
    
    int test_fd = open(log_file, O_RDONLY);
    if (test_fd == -1 && errno == ENOENT) {

        log_fd = creat(log_file, 0644);
        if (log_fd == -1) {
            perror("init_logger: creat log file failed");
            exit(EXIT_FAILURE);
        }
        close(log_fd);
        log_fd = open(log_file, O_WRONLY | O_APPEND, 0644);
        if (log_fd == -1) {
            perror("init_logger: open log file (append) failed");
            exit(EXIT_FAILURE);
        }
    } else {
        if (test_fd != -1) {
            close(test_fd);
        }
        log_fd = open(log_file, O_WRONLY | O_APPEND, 0644);
        if (log_fd == -1) {
            perror("init_logger: open log file failed");
            exit(EXIT_FAILURE);
        }
    }
    
    // Minimalne prawa dostępu - tylko właściciel
    log_sem_id = semget(LOG_SEM_KEY, 1, IPC_CREAT | 0600);
    if (log_sem_id == -1) {
        perror("init_logger: semget log semaphore failed");
        exit(EXIT_FAILURE);
    }
    
    if (semctl(log_sem_id, 0, SETVAL, 1) == -1) {
        perror("init_logger: semctl SETVAL failed");
        exit(EXIT_FAILURE);
    }
}

void log_message(const char *format, ...) {
    char buffer[1024];
    char log_entry[2048];
    va_list args;
    time_t now;
    struct tm *timeinfo;
    pid_t pid = getpid();
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    time(&now);
    timeinfo = localtime(&now);
    
    snprintf(log_entry, sizeof(log_entry), 
             "[%02d:%02d:%02d] [PID:%d] %s\n",
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
             (int)pid, buffer);
    
    if (log_sem_id == -1) {
        log_sem_id = semget(LOG_SEM_KEY, 1, 0);
        if (log_sem_id == -1) {
            fprintf(stdout, "%s", log_entry);
            return;
        }
    }
    
    if (log_fd == -1) {
        log_fd = open("logs/symulacja.log", O_WRONLY | O_APPEND, 0644);
    }
    
    struct sembuf sem_op;
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    
    if (semop(log_sem_id, &sem_op, 1) == -1) {
        fprintf(stdout, "%s", log_entry);
        return;
    }
    
    if (log_fd != -1) {
        ssize_t written = write(log_fd, log_entry, strlen(log_entry));
        if (written == -1) {
            fprintf(stdout, "%s", log_entry);
        }
    } else {
        fprintf(stdout, "%s", log_entry);
    }
    
    sem_op.sem_op = 1;
    semop(log_sem_id, &sem_op, 1);
}

void close_logger(void) {
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}

int create_shared_memory(void) {
    size_t size = sizeof(SharedState);
    
    // Minimalne prawa dostępu - tylko właściciel
    shm_id = shmget(SHM_KEY, size, IPC_CREAT | 0600);
    if (shm_id == -1) {
        perror("create_shared_memory: shmget failed");
        exit(EXIT_FAILURE);
    }
    
    SharedState *state = (SharedState *)shmat(shm_id, NULL, 0);
    if (state == (void *)-1) {
        perror("create_shared_memory: shmat failed");
        exit(EXIT_FAILURE);
    }
    
    memset(state, 0, size);
    state->total_free_seats = MAX_PERSONS;
    state->reserved_seats = 0;
    state->dirty_dishes = 0;
    state->x3_doubled = 0;
    state->effective_x3 = X3;  // Początkowo X3 stolików, może być podwojone
    state->clients_pgid = -1;  // Będzie ustawione przez bar
    state->fire_alarm = 0;     // Brak pożaru na początku
    
    if (shmdt(state) == -1) {
        handle_error("create_shared_memory: shmdt failed");
    }
    
    return shm_id;
}

int create_message_queue(void) {
    // Minimalne prawa dostępu 
    msg_id = msgget(MSG_KEY, IPC_CREAT | 0600);
    if (msg_id == -1) {
        perror("create_message_queue: msgget failed");
        exit(EXIT_FAILURE);
    }
    
    return msg_id;
}

int create_semaphores(void) {
    // Minimalne prawa dostępu 
    sem_id = semget(SEM_KEY, 2, IPC_CREAT | 0600);
    if (sem_id == -1) {
        perror("create_semaphores: semget failed");
        exit(EXIT_FAILURE);
    }
    
    if (semctl(sem_id, SEM_SHARED_STATE, SETVAL, 1) == -1) {
        perror("create_semaphores: semctl SETVAL SEM_SHARED_STATE failed");
        exit(EXIT_FAILURE);
    }
    
    if (semctl(sem_id, SEM_FREE_SEATS, SETVAL, MAX_PERSONS) == -1) {
        perror("create_semaphores: semctl SETVAL SEM_FREE_SEATS failed");
        exit(EXIT_FAILURE);
    }
    
    return sem_id;
}

void cleanup_ipc(void) {
    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
        shm_id = -1;
    }
    
    if (msg_id != -1) {
        msgctl(msg_id, IPC_RMID, NULL);
        msg_id = -1;
    }
    
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
        sem_id = -1;
    }
    
    if (log_sem_id != -1) {
        semctl(log_sem_id, 0, IPC_RMID);
        log_sem_id = -1;
    }
    
    close_logger();
    
    const char *log_file = "logs/symulacja.log";
    if (unlink(log_file) == -1) {
        if (errno != ENOENT) {
            perror("cleanup_ipc: unlink log file failed");
        }
    }
}

SharedState* get_shared_memory(void) {
    int id = shmget(SHM_KEY, 0, 0);
    if (id == -1) {
        handle_error("get_shared_memory: shmget failed");
    }
    
    SharedState *state = (SharedState *)shmat(id, NULL, 0);
    if (state == (void *)-1) {
        handle_error("get_shared_memory: shmat failed");
    }
    
    return state;
}

int get_message_queue(void) {
    int id = msgget(MSG_KEY, 0);
    if (id == -1) {
        handle_error("get_message_queue: msgget failed");
    }
    return id;
}

int get_semaphores(void) {
    int id = semget(SEM_KEY, 0, 0);
    if (id == -1) {
        handle_error("get_semaphores: semget failed");
    }
    return id;
}
