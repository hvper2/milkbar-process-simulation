#include "common.h"
#include "utils.h"
#include <pthread.h>

static int msg_queue_id = -1;
static int group_id = 0;
static int group_size = 0;
static int running = 1;

typedef struct {
    int member_id;
    int group_id;
    volatile int *can_eat;
    volatile int *can_exit;
    volatile int *running;
} MemberArgs;

static pthread_t *member_threads = NULL;
static MemberArgs *member_args = NULL;
static volatile int can_start_eating = 0;
static volatile int can_exit_flag = 0;

// Funkcja sprawdzająca flagę pożaru
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

// Funkcja wątku członka grupy
static void *member_thread_func(void *arg) {
    MemberArgs *args = (MemberArgs *)arg;
    
    while (!(*args->can_eat) && (*args->running)) {
        usleep(10000);
    }
    
    if (!(*args->running)) {
        return NULL;
    }
    
    while (!(*args->can_exit) && (*args->running)) {
        usleep(10000);
    }
    
    return NULL;
}

// Funkcja czyszcząca wątki
static void cleanup_threads(void) {
    if (group_size > 1 && member_threads != NULL) {
        can_exit_flag = 1;
        
        for (int i = 0; i < group_size - 1; i++) {
            pthread_join(member_threads[i], NULL);
        }
    }
    
    if (member_threads != NULL) {
        free(member_threads);
        member_threads = NULL;
    }
    if (member_args != NULL) {
        free(member_args);
        member_args = NULL;
    }
}

// Funkcja obsługująca sygnały
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
    can_exit_flag = 1;
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
    
    log_message("KLIENT #%d: Grupa %d-osobowa wchodzi do baru", group_id, group_size);
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("KLIENT: get_message_queue failed");
    }
    
    // Tworzenie watkow dla czlonkow grupy
    if (group_size > 1) {
        member_threads = malloc((group_size - 1) * sizeof(pthread_t));
        member_args = malloc((group_size - 1) * sizeof(MemberArgs));
        
        if (member_threads == NULL || member_args == NULL) {
            handle_error("KLIENT: malloc failed");
        }
        
        for (int i = 0; i < group_size - 1; i++) {
            member_args[i].member_id = i + 1;   
            member_args[i].group_id = group_id;
            member_args[i].can_eat = &can_start_eating;
            member_args[i].can_exit = &can_exit_flag;
            member_args[i].running = &running;
            
            if (pthread_create(&member_threads[i], NULL, member_thread_func, &member_args[i]) != 0) {
                perror("KLIENT: pthread_create failed");
                running = 0;
                can_exit_flag = 1;
                for (int j = 0; j < i; j++) {
                    pthread_join(member_threads[j], NULL);
                }
                free(member_threads);
                free(member_args);
                return EXIT_FAILURE;
            }
        }
    }
    
    // 5% szansa ze klient nie zamawia
    int orders = (rand() % 100) < NO_ORDER_PROBABILITY ? 0 : 1;
    
    if (!orders) {
        log_message("KLIENT #%d: Nie zamawia - wychodzi (5%% przypadek)", group_id);
        running = 0;
        cleanup_threads();
        return EXIT_SUCCESS;
    }
    
    // Wysyłanie żądania rezerwacji stolika
    Message seat_request;
    seat_request.mtype = MSG_TYPE_SEAT_REQUEST;
    seat_request.group_id = group_id;
    seat_request.group_size = group_size;
    seat_request.table_type = 0;
    seat_request.table_index = 0;
    
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    if (msgsnd(msg_queue_id, &seat_request, msg_size, 0) == -1) {
        perror("KLIENT: msgsnd (rezerwacja) failed");
        running = 0;
        cleanup_threads();
        return EXIT_FAILURE;
    }
    
    // Oczekiwanie na odpowiedz
    Message seat_response;
    long reply_type = 1000 + group_id;
    
    ssize_t received = msgrcv(msg_queue_id, &seat_response, msg_size, reply_type, 0);
    if (received == -1) {
        if (errno == EINTR && !running) {
            if (check_fire_alarm()) {
                log_message("KLIENT #%d: POŻAR! Ewakuacja", group_id);
            }
            cleanup_threads();
            return EXIT_SUCCESS;
        }
        log_message("KLIENT #%d: Błąd msgrcv (oczekiwanie na stolik): %s", group_id, strerror(errno));
        running = 0;
        cleanup_threads();
        return EXIT_FAILURE;
    }
    
    if (seat_response.table_type == 0 || seat_response.table_index < 0) {
        log_message("KLIENT #%d: Brak wolnych miejsc - wychodzi BEZ odbierania dania", group_id);
        running = 0;
        cleanup_threads();
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Stolik %d-os. zarezerwowany -> płaci", 
               group_id, seat_response.table_type);
    
    // Platnosc
    Message payment_msg;
    payment_msg.mtype = MSG_TYPE_PAYMENT;
    payment_msg.group_id = group_id;
    payment_msg.group_size = group_size;
    payment_msg.table_type = seat_response.table_type;
    payment_msg.table_index = seat_response.table_index;
    
    if (msgsnd(msg_queue_id, &payment_msg, msg_size, 0) == -1) {
        if (!running) {
            cleanup_threads();
            return EXIT_SUCCESS;
        }
        perror("KLIENT: msgsnd (płatność) failed");
        running = 0;
        cleanup_threads();
        return EXIT_FAILURE;
    }
    
    // Oczekiwanie na potwierdzenie platnosci
    Message payment_response;
    long payment_reply_type = 2000 + group_id;
    
    ssize_t payment_received = msgrcv(msg_queue_id, &payment_response, msg_size, payment_reply_type, 0);
    if (payment_received == -1) {
        if (errno == EINTR && !running) {
            if (check_fire_alarm()) {
                log_message("KLIENT #%d: POŻAR! Ewakuacja", group_id);
            }
            cleanup_threads();
            return EXIT_SUCCESS;
        }
        log_message("KLIENT #%d: Błąd oczekiwania na potwierdzenie płatności", group_id);
        running = 0;
        cleanup_threads();
        return EXIT_FAILURE;
    }
    
    log_message("KLIENT #%d: Płatność przyjęta -> odbiera danie", group_id);
    
    sleep(1);
    
    if (!running) {
        if (check_fire_alarm()) {
            log_message("KLIENT #%d: POŻAR! Ewakuacja", group_id);
        }
        cleanup_threads();
        return EXIT_SUCCESS;
    }
    
    // Sygnalizacja watkom ze mozna jesc
    can_start_eating = 1;
    
    log_message("KLIENT #%d: Rozpoczyna jedzenie (czas: %ds)", group_id, EATING_TIME);
    
    sleep(EATING_TIME);
    
    if (!running) {
        if (check_fire_alarm()) {
            log_message("KLIENT #%d: POŻAR! Ewakuacja", group_id);
        }
        cleanup_threads();
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Skończył jeść", group_id);
    
    // Zakonczenie watkow
    can_exit_flag = 1;
    
    if (group_size > 1 && member_threads != NULL) {
        for (int i = 0; i < group_size - 1; i++) {
            pthread_join(member_threads[i], NULL);
        }
    }
    
    // Oddanie naczyn
    Message dishes_msg;
    dishes_msg.mtype = MSG_TYPE_DISHES;
    dishes_msg.group_id = group_id;
    dishes_msg.group_size = group_size;
    dishes_msg.table_type = 0;
    dishes_msg.table_index = -1;
    
    if (msgsnd(msg_queue_id, &dishes_msg, msg_size, 0) == -1) {
        if (!running) {
            if (member_threads) free(member_threads);
            if (member_args) free(member_args);
            return EXIT_SUCCESS;
        }
        perror("KLIENT: msgsnd (naczynia) failed");
    }
    
    log_message("KLIENT #%d: Oddał naczynia (%d szt.) i wychodzi z baru", group_id, group_size);
    
    if (member_threads) free(member_threads);
    if (member_args) free(member_args);
    
    return EXIT_SUCCESS;
}
