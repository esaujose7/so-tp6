#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h> // For nanosleep

// --- Constants and Structures ---
#define MAX_NAME_LEN 30
#define PROMPT_HEIGHT 3
#define STATUS_HEIGHT 15 // Not strictly used for fixed height, but for general status area

// Structure to hold client information in the waiting room
typedef struct {
    char name[MAX_NAME_LEN];
    int haircut_time; // Time in seconds
    int barber_id;    // Which barber is cutting this client's hair
} ClientData;

// --- Global State Variables ---
int N_BARBERS;          // Number of barbers
int M_WAITING_CHAIRS;   // Number of waiting chairs
int clients_waiting_outside_count = 0; // Count of clients who couldn't find a chair

// Shared buffer for waiting chairs (circular buffer)
ClientData waiting_room[20]; // Max capacity, should be >= M_WAITING_CHAIRS
int waiting_in = 0;  // Index for adding clients
int waiting_out = 0; // Index for barbers taking clients

// Lists for TUI display
char display_waiting[20][MAX_NAME_LEN]; // Clients in waiting chairs
char display_cutting[20][MAX_NAME_LEN]; // Clients currently being cut
char display_afuera[100][MAX_NAME_LEN]; // Clients waiting outside

// --- Synchronization Mechanisms ---
sem_t sem_customers;    // Counts customers in the waiting room (barbers wait on this)
sem_t sem_barbers;      // Counts available barbers (customers wait on this)
sem_t sem_waiting_chairs; // Counts available waiting chairs (customers wait on this)
pthread_mutex_t mutex_access_chairs; // Mutex for accessing waiting_room and related counters
pthread_mutex_t mutex_ncurses;       // Mutex for all ncurses operations and display lists

// --- TUI (ncurses) Functions ---
void draw_tui() {
    // This function MUST be called within pthread_mutex_lock/unlock(&mutex_ncurses)
    clear();
    box(stdscr, 0, 0);
    
    int current_len; // Declare once

    // Title
    mvprintw(0, (COLS - strlen("PELUTIU - PELUQUERIA INTERACTIVA (v2)")) / 2, "PELUTIU - PELUQUERIA INTERACTIVA (v2)");

    // Section ESPERANDO (Afuera)
    mvprintw(2, 2, "ESPERANDO (Afuera):");
    mvprintw(3, 2, "%d:", clients_waiting_outside_count);

    char afuera_str[256] = "";
    current_len = 0;
    for (int i = 0; i < clients_waiting_outside_count; i++) {
        if (strlen(display_afuera[i]) > 0) {
            current_len += snprintf(afuera_str + current_len, sizeof(afuera_str) - current_len, "%s, ", display_afuera[i]);
        }
    }
    if (clients_waiting_outside_count > 0) {
        afuera_str[current_len - 2] = '\0'; // Remove trailing comma and space
    } else {
        snprintf(afuera_str, sizeof(afuera_str), "Nadie");
    }
    mvprintw(3, 15, "%s", afuera_str);

    // Section SENTADOS
    int current_waiting_clients;
    sem_getvalue(&sem_customers, &current_waiting_clients);
    mvprintw(5, 2, "SENTADOS:");
    mvprintw(6, 2, "%d / %d:", current_waiting_clients, M_WAITING_CHAIRS);
    
    char sentados_str[256] = "";
    current_len = 0;
    for (int i = 0; i < M_WAITING_CHAIRS; i++) {
        if (strlen(display_waiting[i]) > 0) {
            current_len += snprintf(sentados_str + current_len, sizeof(sentados_str) - current_len, "%s, ", display_waiting[i]);
        }
    }
    if (current_waiting_clients > 0) {
        sentados_str[current_len - 2] = '\0'; // Remove trailing comma and space
    } else {
        snprintf(sentados_str, sizeof(sentados_str), "Nadie");
    }
    mvprintw(6, 15, "%s", sentados_str);

    // Section CORTANDO
    int busy_barbers;
    sem_getvalue(&sem_barbers, &busy_barbers); // Get value of available barbers
    busy_barbers = N_BARBERS - busy_barbers; // Calculate busy barbers
    mvprintw(8, 2, "CORTANDO:");
    mvprintw(9, 2, "%d / %d:", busy_barbers, N_BARBERS);

    char cortando_str[256] = "";
    current_len = 0;
    for (int i = 0; i < N_BARBERS; i++) {
        if (strlen(display_cutting[i]) > 0) {
            current_len += snprintf(cortando_str + current_len, sizeof(cortando_str) - current_len, "%s, ", display_cutting[i]);
        }
    }
    if (busy_barbers > 0) {
        cortando_str[current_len - 2] = '\0'; // Remove trailing comma and space
    } else {
        snprintf(cortando_str, sizeof(cortando_str), "Nadie");
    }
    mvprintw(9, 15, "%s", cortando_str);

    // Prompt de entrada
    mvprintw(LINES - PROMPT_HEIGHT + 1, 2, "Ingress Cliente: ");
    move(LINES - PROMPT_HEIGHT + 1, 19); // Position cursor for input
    
    refresh();
}

// --- Barber Thread ---
void* barber_thread(void* arg) {
    int barber_id = *(int*)arg;
    ClientData current_client;

    while (1) {
        // 1. Wait for a customer to arrive (sleep if no customers)
        sem_wait(&sem_customers);

        // 2. Acquire mutex to access shared waiting room
        pthread_mutex_lock(&mutex_access_chairs);

        // 3. Take a customer from the waiting room
        current_client = waiting_room[waiting_out];
        waiting_out = (waiting_out + 1) % M_WAITING_CHAIRS;

        // 4. Release a waiting chair
        sem_post(&sem_waiting_chairs);

        // 5. Update TUI display lists (protected by ncurses mutex)
        pthread_mutex_lock(&mutex_ncurses);
        strcpy(display_cutting[barber_id], current_client.name);
        // Clear from waiting display
        for(int i=0; i<M_WAITING_CHAIRS; i++) {
            if(strcmp(display_waiting[i], current_client.name) == 0) {
                display_waiting[i][0] = '\0';
                break;
            }
        }
        // If not found, it means it was a client from outside who immediately took a chair
        // This logic is a bit tricky with the current display_waiting.
        // A simpler approach for display_waiting is to just iterate the circular buffer.
        // For now, let's just clear the first empty slot.
        // The current_waiting_clients calculation in draw_tui is more accurate.
        draw_tui();
        pthread_mutex_unlock(&mutex_ncurses);

        // 6. Release mutex for waiting room
        pthread_mutex_unlock(&mutex_access_chairs);

        // 7. Cut hair (simulate work)
        sleep(current_client.haircut_time);

        // 8. Haircut finished. Release barber slot.
        sem_post(&sem_barbers);

        // 9. Update TUI display (protected by ncurses mutex)
        pthread_mutex_lock(&mutex_ncurses);
        display_cutting[barber_id][0] = '\0'; // Clear barber's cutting slot
        draw_tui();
        pthread_mutex_unlock(&mutex_ncurses);
    }
    return NULL;
}

// --- Client Thread ---
void* client_thread(void* arg) {
    ClientData* info = (ClientData*)arg; // Client's name and haircut time
    int was_outside = 0;

    // 1. Try to get a waiting chair
    if (sem_trywait(&sem_waiting_chairs) != 0) {
        // No chairs available, must wait outside.
        was_outside = 1;
        pthread_mutex_lock(&mutex_ncurses);
        strcpy(display_afuera[clients_waiting_outside_count], info->name);
        clients_waiting_outside_count++;
        draw_tui();
        pthread_mutex_unlock(&mutex_ncurses);

        // Blocking wait for a chair to become free
        sem_wait(&sem_waiting_chairs);
    }

    // At this point, we have secured a waiting chair spot.
    if (was_outside) {
        // If we were waiting outside, update the display lists.
        pthread_mutex_lock(&mutex_ncurses);
        clients_waiting_outside_count--;
        // Remove name from the display list
        int found = 0;
        for(int i=0; i<99; i++) {
            if(found || strcmp(display_afuera[i], info->name) == 0) {
                found = 1;
                strcpy(display_afuera[i], display_afuera[i+1]);
            }
        }
        display_afuera[99][0] = '\0';
        draw_tui();
        pthread_mutex_unlock(&mutex_ncurses);
    }

    // 2. Acquire mutex to access shared waiting room
    pthread_mutex_lock(&mutex_access_chairs);
    // 3. Place client in waiting room
    waiting_room[waiting_in] = *info;
    waiting_in = (waiting_in + 1) % M_WAITING_CHAIRS;
    pthread_mutex_unlock(&mutex_access_chairs);

    // 6. Signal that a customer has arrived
    sem_post(&sem_customers);

    // 4. Update TUI display lists (protected by ncurses mutex)
    pthread_mutex_lock(&mutex_ncurses);
    for(int i=0; i<M_WAITING_CHAIRS; i++) {
        if(display_waiting[i][0] == '\0') {
            strcpy(display_waiting[i], info->name);
            break;
        }
    }
    draw_tui();
    pthread_mutex_unlock(&mutex_ncurses);

    // 7. Wait for an available barber
    sem_wait(&sem_barbers);

    // Client thread is done.
    free(info);
    return NULL;
}

// --- Main Function ---
int main(int argc, char *argv[]) {
    // 1. Validate and get arguments
    if (argc != 3) {
        fprintf(stderr, "Uso: pelutiu_v2 <num_peluqueros> <num_sillas_espera>\n");
        fprintf(stderr, "Ejemplo: pelutiu_v2 2 4\n");
        return 1;
    }
    N_BARBERS = atoi(argv[1]);
    M_WAITING_CHAIRS = atoi(argv[2]);

    if (N_BARBERS <= 0 || M_WAITING_CHAIRS <= 0) {
        fprintf(stderr, "Error: El número de peluqueros y sillas de espera debe ser mayor que 0.\n");
        return 1;
    }
    if (M_WAITING_CHAIRS > 20) { // Limit by array size
        fprintf(stderr, "Error: El número máximo de sillas de espera es 20.\n");
        return 1;
    }
    if (N_BARBERS > 20) { // Limit by array size
        fprintf(stderr, "Error: El número máximo de peluqueros es 20.\n");
        return 1;
    }
    
    // 2. Initialize ncurses
    initscr();             
    cbreak();              
    noecho();              
    keypad(stdscr, TRUE);  
    
    // 3. Initialize synchronization primitives
    sem_init(&sem_customers, 0, 0);             // No customers initially
    sem_init(&sem_barbers, 0, N_BARBERS);       // All barbers are free initially
    sem_init(&sem_waiting_chairs, 0, M_WAITING_CHAIRS); // All waiting chairs are free
    pthread_mutex_init(&mutex_access_chairs, NULL);
    pthread_mutex_init(&mutex_ncurses, NULL);
    
    // 4. Initialize display lists
    for(int i=0; i<M_WAITING_CHAIRS; i++) display_waiting[i][0] = '\0';
    for(int i=0; i<N_BARBERS; i++) display_cutting[i][0] = '\0';
    for(int i=0; i<100; i++) display_afuera[i][0] = '\0';

    // 5. Create barber threads
    pthread_t barber_tids[N_BARBERS];
    int barber_ids[N_BARBERS];
    for (int i = 0; i < N_BARBERS; i++) {
        barber_ids[i] = i; // Assign a unique ID to each barber
        pthread_create(&barber_tids[i], NULL, barber_thread, &barber_ids[i]);
    }
    
    // 6. Main loop for TUI and input
    char input_buffer[100];
    while (1) {
        // a. Draw current state
        pthread_mutex_lock(&mutex_ncurses);
        draw_tui();
        pthread_mutex_unlock(&mutex_ncurses);
        
        // b. Capture user input (Name Time)
        move(LINES - PROMPT_HEIGHT + 1, 19); 
        echo(); // Temporarily enable echo for input
        getstr(input_buffer); // Blocks until ENTER
        noecho(); // Disable echo

        char name[MAX_NAME_LEN];
        int time;
        
        // c. Parse input
        if (strcmp(input_buffer, "exit") == 0) {
            break; // Exit main loop
        }

        if (sscanf(input_buffer, "%s %d", name, &time) == 2) {
            ClientData* info = (ClientData*)malloc(sizeof(ClientData));
            strncpy(info->name, name, MAX_NAME_LEN - 1);
            info->name[MAX_NAME_LEN - 1] = '\0'; // Ensure null termination
            info->haircut_time = time;
            
            // d. Create client thread
            pthread_t client_tid;
            pthread_create(&client_tid, NULL, client_thread, (void*)info);
            pthread_detach(client_tid); // Don't join client threads
        } else { // Handle invalid input
            pthread_mutex_lock(&mutex_ncurses);
            mvprintw(LINES - 1, 2, "Entrada inválida. Use: <nombre> <tiempo> o 'exit'");
            draw_tui(); // Redraw to clear previous prompt and show message
            pthread_mutex_unlock(&mutex_ncurses);
            sleep(1); // Pause for user to see message
        }
    }

    // 7. Cleanup (will be reached now)
    endwin();
    sem_destroy(&sem_customers);
    sem_destroy(&sem_barbers);
    sem_destroy(&sem_waiting_chairs);
    pthread_mutex_destroy(&mutex_access_chairs);
    pthread_mutex_destroy(&mutex_ncurses);
    
    // For detached threads, no need to join.
    // If barbers were joinable, we'd need a way to signal them to exit.
    // For a daemon-like program, they usually run indefinitely.

    return 0;
}
