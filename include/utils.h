#ifndef UTILS_H
#define UTILS_H

#include "common.h"

/**
 * Inicjalizuje logger - tworzy plik logu i semafor synchronizujący zapis.
 * Plik logu: logs/symulacja.log 
 */
void init_logger(void);

/**
 * Zamyka logger - zwalnia deskryptor pliku logu.
 */
void close_logger(void);

/**
 * Zapisuje sformatowany komunikat do pliku logu z timestampem i PID.
 * Format: [HH:MM:SS] [PID:xxxxx] wiadomość
 * @param format - format printf
 * @param ... - argumenty formatu
 */
void log_message(const char *format, ...);

/**
 * Tworzy segment pamięci współdzielonej dla stanu sali (SharedState).
 * Inicjalizuje strukturę wartościami początkowymi.
 * @return ID segmentu pamięci współdzielonej
 */
int create_shared_memory(void);

/**
 * Tworzy kolejkę komunikatów IPC dla wymiany wiadomości między procesami.
 * @return ID kolejki komunikatów
 */
int create_message_queue(void);

/**
 * Tworzy zbiór semaforów do synchronizacji dostępu do zasobów.
 * Inicjalizuje semafor SEM_SHARED_STATE wartością 1 (mutex).
 * @return ID zbioru semaforów
 */
int create_semaphores(void);

/**
 * Pobiera wskaźnik do istniejącej pamięci współdzielonej.
 * @return wskaźnik do SharedState lub NULL w przypadku błędu
 */
SharedState* get_shared_memory(void);

/**
 * Pobiera ID istniejącej kolejki komunikatów.
 * @return ID kolejki komunikatów lub -1 w przypadku błędu
 */
int get_message_queue(void);

/**
 * Pobiera ID istniejącego zbioru semaforów.
 * @return ID zbioru semaforów lub -1 w przypadku błędu
 */
int get_semaphores(void);

/**
 * Zwalnia wszystkie zasoby IPC (pamięć współdzielona, kolejka, semafory).
 * Wywołuje także close_logger().
 */
void cleanup_ipc(void);

/**
 * Wypisuje komunikat błędu z perror() i kończy program z EXIT_FAILURE.
 * @param msg - komunikat błędu do wyświetlenia
 */
void handle_error(const char *msg);

#endif // UTILS_H
