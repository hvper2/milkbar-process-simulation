#include "common.h"
#include "utils.h"

static int msg_queue_id = -1;
static int running = 1;

void signal_handler(int sig) {
    log_message("KASJER: Otrzymano sygnał %d - kończę pracę", sig);
    running = 0;
}

int main(void) {
    log_message("KASJER: Start pracy kasjera");
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("KASJER: get_message_queue failed");
    }
    
    log_message("KASJER: Połączono z kolejką komunikatów (msg_id=%d)", msg_queue_id);
    
    Message msg;
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
    log_message("KASJER: Rozpoczęcie obsługi płatności");
    
    while (running) {
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
        
        log_message("KASJER: Otrzymano płatność od grupy #%d (rozmiar=%d)", 
                   msg.group_id, msg.group_size);
        
        sleep(1);
        
        log_message("KASJER: Płatność przetworzona - wydaję potwierdzenie dla grupy #%d", 
                   msg.group_id);
        
        Message paid_msg;
        paid_msg.mtype = MSG_TYPE_PAID;
        paid_msg.group_id = msg.group_id;
        paid_msg.group_size = msg.group_size;
        paid_msg.table_type = 0;
        paid_msg.table_index = 0;
        
        if (msgsnd(msg_queue_id, &paid_msg, msg_size, 0) == -1) {
            log_message("KASJER: Błąd msgsnd (MSG_TYPE_PAID): %s", strerror(errno));
            if (!running) {
                break;
            }
            continue;
        }
        
        log_message("KASJER: Wysłano potwierdzenie płatności do obsługi (grupa #%d)", 
                   msg.group_id);
    }
    
    log_message("KASJER: Kończenie pracy kasjera");
    
    return EXIT_SUCCESS;
}
