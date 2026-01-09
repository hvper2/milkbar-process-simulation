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
    
    // Rezerwacja stolika
    Message seat_request;
    seat_request.mtype = MSG_TYPE_SEAT_REQUEST;
    seat_request.group_id = group_id;
    seat_request.group_size = group_size;
    seat_request.table_type = 0;
    seat_request.table_index = 0;
    
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    if (msgsnd(msg_queue_id, &seat_request, msg_size, 0) == -1) {
        log_message("KLIENT #%d: Błąd msgsnd (rezerwacja): %s", group_id, strerror(errno));
        return EXIT_FAILURE;
    }
    
    log_message("KLIENT #%d: Wysłano prośbę o rezerwację stolika", group_id);
    
    // Czekamy na odpowiedź od obsługi (typ = 1000 + group_id)
    Message seat_response;
    long reply_type = 1000 + group_id;
    
    ssize_t received = msgrcv(msg_queue_id, &seat_response, msg_size, reply_type, 0);
    if (received == -1) {
        if (errno == EINTR && !running) {
            return EXIT_SUCCESS;
        }
        log_message("KLIENT #%d: Błąd msgrcv (oczekiwanie na stolik): %s", group_id, strerror(errno));
        return EXIT_FAILURE;
    }
    
    // Sprawdź czy dostaliśmy stolik
    if (seat_response.table_type == 0 || seat_response.table_index < 0) {
        log_message("KLIENT #%d: Brak wolnych miejsc - wychodzi BEZ odbierania dania", group_id);
        return EXIT_SUCCESS;
    }
    
    log_message("KLIENT #%d: Stolik %d-os. (indeks %d) ZAREZERWOWANY - idę płacić", 
               group_id, seat_response.table_type, seat_response.table_index);
    
    
    // Płatność
    Message payment_msg;
    payment_msg.mtype = MSG_TYPE_PAYMENT;
    payment_msg.group_id = group_id;
    payment_msg.group_size = group_size;
    payment_msg.table_type = seat_response.table_type;
    payment_msg.table_index = seat_response.table_index;
    
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
