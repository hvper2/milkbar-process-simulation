#include "common.h"
#include "utils.h"

static int msg_queue_id = -1;
static SharedState *shared_state = NULL;
static int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static int check_fire_alarm(void) {
    if (shared_state != NULL) {
        return shared_state->fire_alarm;
    }
    return 0;
}

int main(void) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("KASJER: get_message_queue failed");
    }
    
    shared_state = get_shared_memory();
    if (shared_state == NULL) {
        handle_error("KASJER: get_shared_memory failed");
    }
    
    log_message("KASJER: Kasa otwarta, czekam na klientów");
    
    Message msg;
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    while (running) {
        // Sprawdź flagę pożaru przed czekaniem na wiadomość
        if (check_fire_alarm()) {
            break;
        }
        
        ssize_t received = msgrcv(msg_queue_id, &msg, msg_size, MSG_TYPE_PAYMENT, 0);
        
        if (received == -1) {
            if (errno == EINTR) {
                if (!running) {
                    break;
                }
                continue;
            } else {
                log_message("KASJER: Błąd msgrcv: %s", strerror(errno));
                if (!running) {
                    break;
                }
                continue;
            }
        }
        
        if (check_fire_alarm()) {
            break;
        }
        
        log_message("KASJER: Otrzymał płatność od grupy #%d (rozmiar: %d)", msg.group_id, msg.group_size);
        
        sleep(2);
        
        // Sprawdź flagę pożaru po sleep, przed wysłaniem odpowiedzi
        if (check_fire_alarm()) {
            break;
        }
        
        Message paid_msg;
        paid_msg.mtype = 2000 + msg.group_id;
        paid_msg.group_id = msg.group_id;
        paid_msg.group_size = msg.group_size;
        paid_msg.table_type = 0;
        paid_msg.table_index = 0;
        
        if (msgsnd(msg_queue_id, &paid_msg, msg_size, 0) == -1) {
            if (!running) break;
            continue;
        }
        
        log_message("KASJER: Płatność przetworzona - grupa #%d może odebrać danie", msg.group_id);
    }
    
    log_message("KASJER: Kasa zamknięta");
    
    if (shared_state != NULL) {
        shmdt(shared_state);
    }
    
    return EXIT_SUCCESS;
}
