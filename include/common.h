#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>


// Liczba stolików każdego typu
#define X1 4  // stoliki 1-osobowe
#define X2 3  // stoliki 2-osobowe
#define X3 2  // stoliki 3-osobowe (bazowa liczba)
#define X3_MAX (X3 * 2)  // maksymalna liczba po podwojeniu
#define X4 2  // stoliki 4-osobowe

// Maksymalna liczba osób w sali: N = X1*1 + X2*2 + X3*3 + X4*4
#define MAX_PERSONS (X1*1 + X2*2 + X3*3 + X4*4)
#define MAX_PERSONS_DOUBLED (X1*1 + X2*2 + X3_MAX*3 + X4*4)

// Czas symulacji (w sekundach)
#define SIMULATION_TIME 30

// Liczba klientów (grup) do wygenerowania na starcie symulacji
#define TOTAL_CLIENTS 30

// Prawdopodobieństwo, że klient nie zamawia (w %)
#define NO_ORDER_PROBABILITY 5

// Czas jedzenia klienta (w sekundach)
#define EATING_TIME 3

// Czas wywołania sygnałów przez kierownika (w sekundach od startu symulacji)
#define SIGNAL1_TIME 10   // Sygnał 1 (SIGUSR1)
#define SIGNAL2_TIME 15   // Sygnał 2 (SIGUSR2)
#define SIGNAL3_TIME 29  // Sygnał 3 (SIGTERM) 

// Liczba stolików do rezerwacji przez kierownika (sygnał 2)
#define RESERVED_TABLE_COUNT 2

// Klucz bazowy dla zasobów IPC
#define IPC_KEY_BASE 0x00001010
#define SHM_KEY (IPC_KEY_BASE + 1) 
#define MSG_KEY (IPC_KEY_BASE + 2)
#define SEM_KEY (IPC_KEY_BASE + 3)  
#define LOG_SEM_KEY (IPC_KEY_BASE + 4) 

// structura przechowująca stan sali w pamięci dzielonej
typedef struct {
    int table_1[X1];      // 0 = wolny, 1 = zajęty (grupa 1-os.), -1 = zarezerwowany
    int table_2[X2];      // 0 = wolny, 1-2 = zajęte miejsca (grupa 2-os.), -1 = zarezerwowany
    int table_3[X3_MAX];  // 0 = wolny, 1-3 = zajęte miejsca (tablica na podwojenie), -1 = zarezerwowany
    int table_4[X4];      // 0 = wolny, 1-4 = zajęte miejsca (grupa 4-os.), -1 = zarezerwowany
    
    // Tablice group_id dla każdego miejsca przy stoliku (dla wizualizacji)
    int table_1_groups[X1][1];          
    int table_2_groups[X2][2];          
    int table_3_groups[X3_MAX][3];      
    int table_4_groups[X4][4];        
    
    int reserved_seats;   // Liczba zarezerwowanych miejsc (przez kierownika)
    int dirty_dishes;     // Licznik brudnych naczyń
    int x3_doubled;       // Flaga: czy X3 już zostało podwojone (0/1)
    int effective_x3;     // Aktualna liczba stolików 3-os. (X3 lub X3*2)
    
    int total_free_seats; // Aktualna liczba wolnych miejsc
    
    pid_t clients_pgid;   // PGID grupy klientów (do sygnalizacji pożaru)
    int fire_alarm;       // Flaga pożaru (1 = pożar)
    
    // Kolejka oczekujących klientów (dla wizualizacji)
    #define MAX_WAITING_GROUPS 50
    int waiting_group_ids[MAX_WAITING_GROUPS];    // ID grup czekających
    int waiting_group_sizes[MAX_WAITING_GROUPS];  // Rozmiary grup czekających
    int waiting_count;                            // Liczba grup w kolejce
} SharedState;

//  kolejka komunikatów 
#define MSG_TYPE_PAYMENT 1        // Klient → Kasjer: "chcę zapłacić"
#define MSG_TYPE_DISHES 3         // Klient → Obsługa: "oddajemy naczynia"
#define MSG_TYPE_SEAT_REQUEST 4   // Klient → Obsługa: "rezerwuj stolik dla grupy"
#define MSG_TYPE_SEAT_CONFIRM 5   // Obsługa → Klient: "stolik zarezerwowany"
#define MSG_TYPE_SEAT_REJECT 6    // Obsługa → Klient: "brak miejsca"
#define MSG_TYPE_RESERVE_SEATS 7  // Kierownik → Obsługa: "zarezerwuj N miejsc"

// Struktura wiadomości
typedef struct {
    long mtype;           // Typ wiadomości
    int group_id;         // ID grupy klienta
    int group_size;       // Rozmiar grupy
    int table_type;       // Typ stolika
    int table_index;      // Indeks stolika w tablicy
} Message;

#define SEM_SHARED_STATE 0    // Mutex na pamięć dzieloną

#endif // COMMON_H

