#ifndef UTILS_H
#define UTILS_H

#include "common.h"

void init_logger(void);

void close_logger(void);

void log_message(const char *format, ...);

int create_shared_memory(void);

int create_message_queue(void);

int create_semaphores(void);

SharedState* get_shared_memory(void);

int get_message_queue(void);

int get_semaphores(void);

void cleanup_ipc(void);

void handle_error(const char *msg);

#endif // UTILS_H
