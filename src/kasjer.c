#include "common.h"
#include "utils.h"

static int msg_queue_id = -1;
static int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    msg_queue_id = get_message_queue();
    if (msg_queue_id == -1) {
        handle_error("KASJER: get_message_queue failed");
    }
    
    Message msg;
    ssize_t msg_size = sizeof(Message) - sizeof(long);
    
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
        
        log_message("KASJER: Otrzymał płatność od grupy #%d (rozmiar: %d)", msg.group_id, msg.group_size);
        
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
    
    int shm_id = shmget(SHM_KEY, sizeof(SharedState), 0);
    int is_fire = 0;
    if (shm_id != -1) {
        SharedState *state = (SharedState *)shmat(shm_id, NULL, 0);
        if (state != (void *)-1) {
            is_fire = state->fire_alarm;
            shmdt(state);
        }
    }
    
    if (is_fire) {
        log_message("KASJER: Kasa zamknięta (pożar)");
    } else {
        log_message("KASJER: Kasa zamknięta");
    }
    
    return EXIT_SUCCESS;
}
