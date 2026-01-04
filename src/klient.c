#include "common.h"
#include "utils.h"

static int msg_queue_id = -1;
static int group_id = 0;
static int group_size = 0;
static int running = 1;

void signal_handler(int sig) {
    log_message("KLIENT #%d: Otrzymano sygnał pożaru %d - natychmiast wychodzę!", group_id, sig);
    running = 0;
}

int main(void) {
    // Inicjalizacja
    srand(time(NULL) ^ getpid());
    group_id = getpid();
    group_size = (rand() % 3) + 1;
    
    log_message("KLIENT #%d: Grupa %d-osobowa wchodzi do baru", group_id, group_size);
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("KLIENT: get_message_queue failed");
    }
    
    // Losuj czy zamawia (5% nie zamawia)
    int orders = (rand() % 100) < NO_ORDER_PROBABILITY ? 0 : 1;
    
    if (!orders) {
        log_message("KLIENT #%d: Nie zamawia - wychodzi", group_id);
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Zamawia gorące danie", group_id);
    
    // Sprawdź czy jest miejsce (przed płatnością)
    SharedState *state = get_shared_memory();
    if (state == NULL) {
        handle_error("KLIENT: get_shared_memory failed");
    }
    
    int sem_id = get_semaphores();
    if (sem_id == -1) {
        handle_error("KLIENT: get_semaphores failed");
    }
    
    struct sembuf sem_op;
    sem_op.sem_num = SEM_SHARED_STATE;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    
    if (semop(sem_id, &sem_op, 1) == -1) {
        log_message("KLIENT #%d: Błąd semop (sprawdzanie miejsc): %s", group_id, strerror(errno));
        shmdt(state);
        return EXIT_FAILURE;
    }
    
    int has_place = 0;
    if (state->total_free_seats - state->reserved_seats >= group_size) {
        has_place = 1;
    }
    
    sem_op.sem_op = 1;
    semop(sem_id, &sem_op, 1);
    shmdt(state);
    
    if (!has_place) {
        log_message("KLIENT #%d: Brak miejsc - wychodzi (NIE odbiera dania)", group_id);
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Jest miejsce - przechodzi do płatności", group_id);
    
    // Płatność
    Message payment_msg;
    payment_msg.mtype = MSG_TYPE_PAYMENT;
    payment_msg.group_id = group_id;
    payment_msg.group_size = group_size;
    payment_msg.table_type = 0;
    payment_msg.table_index = 0;
    
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    if (msgsnd(msg_queue_id, &payment_msg, msg_size, 0) == -1) {
        log_message("KLIENT #%d: Błąd msgsnd (płatność): %s", group_id, strerror(errno));
        if (!running) {
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }
    
    log_message("KLIENT #%d: Wysłano płatność do kasjera", group_id);
    sleep(1);
    
    if (!running) {
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Odebrał danie (po potwierdzeniu z kasy)", group_id);
    
    // Czekanie na stolik
    sleep(1);
    
    if (!running) {
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Usiadł przy stoliku - zaczyna jeść", group_id);
    
    // Jedzenie
    int eating_time = (rand() % 5) + 3;
    log_message("KLIENT #%d: Je przez %d sekund", group_id, eating_time);
    
    sleep(eating_time);
    
    if (!running) {
        log_message("KLIENT #%d: Przerwano jedzenie (pożar) - wychodzi", group_id);
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Skończył jeść", group_id);
    
    // Oddanie naczyń
    Message dishes_msg;
    dishes_msg.mtype = MSG_TYPE_DISHES;
    dishes_msg.group_id = group_id;
    dishes_msg.group_size = group_size;
    dishes_msg.table_type = 0;
    dishes_msg.table_index = -1;
    
    if (msgsnd(msg_queue_id, &dishes_msg, msg_size, 0) == -1) {
        log_message("KLIENT #%d: Błąd msgsnd (naczynia): %s", group_id, strerror(errno));
        if (!running) {
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }
    
    log_message("KLIENT #%d: Oddano naczynia (1 osoba z grupy)", group_id);
    log_message("KLIENT #%d: Wychodzi z baru", group_id);
    
    return EXIT_SUCCESS;
}
