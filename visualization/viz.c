#include "../include/common.h"
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

// ANSI color codes
#define RESET   "\033[0m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"

// Clear screen
#define CLEAR_SCREEN "\033[2J\033[H"

static SharedState *shared_state = NULL;
static int shm_id = -1;
static int sem_id = -1;

void sem_wait(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}

void sem_signal(int sem_id, int sem_num) {
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}

static int get_group_id(int table_type, int table_index, int seat) {
    switch (table_type) {
        case 1: return shared_state->table_1_groups[table_index][seat];
        case 2: return shared_state->table_2_groups[table_index][seat];
        case 3: return shared_state->table_3_groups[table_index][seat];
        case 4: return shared_state->table_4_groups[table_index][seat];
        default: return 0;
    }
}

static void collect_groups(int table_type, int table_index, int groups[4], int *group_count) {
    *group_count = 0;
    int max_seats = (table_type == 1) ? 1 : (table_type == 2) ? 2 : (table_type == 3) ? 3 : 4;
    
    for (int i = 0; i < max_seats; i++) {
        int gid = get_group_id(table_type, table_index, i);
        if (gid > 0) {
            int found = 0;
            for (int j = 0; j < *group_count; j++) {
                if (groups[j] == gid) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                groups[(*group_count)++] = gid;
            }
        }
    }
}

static int count_group_seats(int table_type, int table_index, int group_id) {
    int max_seats = (table_type == 1) ? 1 : (table_type == 2) ? 2 : (table_type == 3) ? 3 : 4;
    int count = 0;
    
    for (int i = 0; i < max_seats; i++) {
        if (get_group_id(table_type, table_index, i) == group_id) {
            count++;
        }
    }
    return count;
}

void print_table_status(int table_type, int table_index, int occupied, int capacity) {
    if (occupied == -1) {
        printf(RED "[");
        for (int i = 0; i < capacity; i++) {
            printf("X");
        }
        printf(" %d/%d]" RESET, capacity, capacity);
        return;
    }
    
    if (occupied == 0) {
        printf(WHITE "[ ]" RESET);
        return;
    }
    
    int groups[4] = {0, 0, 0, 0};
    int group_count = 0;
    collect_groups(table_type, table_index, groups, &group_count);
    
    printf("[");
    
    if (group_count == 1) {
        for (int j = 0; j < occupied; j++) {
            printf(GREEN "X" RESET);
        }
    } else if (group_count > 1) {
        const char* colors[] = {GREEN, YELLOW, BLUE, CYAN};
        for (int i = 0; i < group_count; i++) {
            int group_seats = count_group_seats(table_type, table_index, groups[i]);
            for (int j = 0; j < group_seats; j++) {
                printf("%sX" RESET, colors[i % 4]);
            }
        }
    }
    
    for (int j = occupied; j < capacity; j++) {
        printf(WHITE "_" RESET);
    }
    
    printf(" %d/%d]", occupied, capacity);
}

void visualize(void) {
    printf(CLEAR_SCREEN);
    sem_wait(sem_id, SEM_SHARED_STATE);
    
    if (shared_state->reserved_seats > 0) {
        printf("  " RED "⚠ Zarezerwowane stoliki: %d miejsc (całe stoliki zarezerwowane do końca symulacji)" RESET "\n", 
               shared_state->reserved_seats);
    }

    sem_signal(sem_id, SEM_SHARED_STATE);
    
    printf(BOLD "\nSTOLIKI:\n" RESET);
    
    sem_wait(sem_id, SEM_SHARED_STATE);
    
    int x3_count = shared_state->effective_x3;
    
    printf("\n  Stoliki 1-osobowe (%d): ", X1);
    for (int i = 0; i < X1; i++) {
        print_table_status(1, i, shared_state->table_1[i], 1);
        printf(" ");
    }
    
    printf("\n  Stoliki 2-osobowe (%d): ", X2);
    for (int i = 0; i < X2; i++) {
        print_table_status(2, i, shared_state->table_2[i], 2);
        printf(" ");
    }
    
    printf("\n  Stoliki 3-osobowe (%d): ", x3_count);
    for (int i = 0; i < x3_count; i++) {
        print_table_status(3, i, shared_state->table_3[i], 3);
        printf(" ");
    }
    if (x3_count < X3_MAX) {
        printf(WHITE " (max: %d)" RESET, X3_MAX);
    }
    
    printf("\n  Stoliki 4-osobowe (%d): ", X4);
    for (int i = 0; i < X4; i++) {
        print_table_status(4, i, shared_state->table_4[i], 4);
        printf(" ");
    }
    
    sem_signal(sem_id, SEM_SHARED_STATE);
    
    fflush(stdout);
}

int main(void) {
    shm_id = shmget(SHM_KEY, sizeof(SharedState), 0);
    if (shm_id == -1) {
        fprintf(stderr, "Błąd: Nie można otworzyć shared memory. Upewnij się, że ./bin/bar jest uruchomiony.\n");
        return EXIT_FAILURE;
    }
    
    shared_state = (SharedState *)shmat(shm_id, NULL, 0);
    if (shared_state == (void *)-1) {
        perror("shmat failed");
        return EXIT_FAILURE;
    }
    
    sem_id = semget(SEM_KEY, 0, 0);
    if (sem_id == -1) {
        fprintf(stderr, "Błąd: Nie można otworzyć semaforów.\n");
        shmdt(shared_state);
        return EXIT_FAILURE;
    }
    
    printf("Wizualizacja uruchomiona. Odświeżanie co 1 sekundę...\n");
    sleep(1);
    
    while (1) {
        visualize();
        sleep(1);
    }
    
    shmdt(shared_state);
    return EXIT_SUCCESS;
}
