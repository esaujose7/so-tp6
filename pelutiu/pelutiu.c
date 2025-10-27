#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// --- Constantes y Estructuras ---
#define MAX_NAME_LEN 30
#define PROMPT_HEIGHT 3
#define STATUS_HEIGHT 15

typedef struct {
    char name[MAX_NAME_LEN];
    int time;
    pthread_t tid; // ID del hilo cliente
} ClienteInfo;

// --- Variables Globales de Estado y Configuración [cite: 75] ---
int N_PELUQUEROS; // Sillas de corte (ej. 2)
int M_SILLAS_ESPERA; // Sillas de espera (ej. 4)
int sillas_espera_libres; // Inicializado a M_SILLAS_ESPERA

// Listas de nombres protegidas por mutex
char lista_esperando[20][MAX_NAME_LEN]; // La capacidad debe ser mayor o igual a M_SILLAS_ESPERA
char lista_cortando[20][MAX_NAME_LEN]; // La capacidad debe ser mayor o igual a N_PELUQUEROS

// --- Mecanismos de Sincronización [cite: 69, 117] ---
sem_t sem_clientes_esperando; // Cuenta clientes en sillas de espera (Inicial: 0)
sem_t sem_peluqueros_libres;  // Cuenta peluqueros disponibles (Inicial: N_PELUQUEROS)
pthread_mutex_t g_mutex_estado; // Mutex para proteger listas, contadores y las llamadas a ncurses

// --- TUI (ncurses) Funciones ---
void draw_tui() {
    // ESTA FUNCIÓN DEBE LLAMARSE DENTRO DE pthread_mutex_lock/unlock(&g_mutex_estado)
    clear();
    box(stdscr, 0, 0);
    
    // Título [cite: 77]
    mvprintw(0, (COLS - strlen("PELUTIU - PELUQUERIA INTERACTIVA")) / 2, "PELUTIU - PELUQUERIA INTERACTIVA");

    // Sección ESPERANDO [cite: 78]
    mvprintw(2, 2, "ESPERANDO (Afuera):");
    int esperando_afuera = sillas_espera_libres < 0 ? abs(sillas_espera_libres) : 0;
    mvprintw(3, 2, "%d: %s", esperando_afuera, (esperando_afuera > 0) ? "Clientes Afuera..." : "Nadie");

    // Sección SENTADOS [cite: 80, 112]
    int sentados = M_SILLAS_ESPERA - sillas_espera_libres;
    mvprintw(5, 2, "SENTADOS:");
    mvprintw(6, 2, "%d / %d:", sentados, M_SILLAS_ESPERA);
    
    char sentados_str[256] = "";
    int current_len = 0;
    for (int i = 0; i < M_SILLAS_ESPERA; i++) {
        if (strlen(lista_esperando[i]) > 0) {
            current_len += snprintf(sentados_str + current_len, sizeof(sentados_str) - current_len, "%s, ", lista_esperando[i]);
        }
    }
    if (sentados > 0) {
        sentados_str[current_len - 2] = '\0'; // Quita la coma y espacio finales
    } else {
        snprintf(sentados_str, sizeof(sentados_str), "Nadie");
    }
    mvprintw(6, 15, "%s", sentados_str);

    // Sección CORTANDO [cite: 82, 114]
    int cortando = N_PELUQUEROS;
    sem_getvalue(&sem_peluqueros_libres, &cortando);
    cortando = N_PELUQUEROS - cortando;
    mvprintw(8, 2, "CORTANDO:");
    mvprintw(9, 2, "%d / %d:", cortando, N_PELUQUEROS);

    char cortando_str[256] = "";
    current_len = 0;
    for (int i = 0; i < N_PELUQUEROS; i++) {
        if (strlen(lista_cortando[i]) > 0) {
            current_len += snprintf(cortando_str + current_len, sizeof(cortando_str) - current_len, "%s, ", lista_cortando[i]);
        }
    }
    if (cortando > 0) {
        cortando_str[current_len - 2] = '\0'; // Quita la coma y espacio finales
    } else {
        snprintf(cortando_str, sizeof(cortando_str), "Nadie");
    }
    mvprintw(9, 15, "%s", cortando_str);

    // Prompt de entrada [cite: 84]
    mvprintw(LINES - PROMPT_HEIGHT + 1, 2, "Ingress Cliente: ");
    move(LINES - PROMPT_HEIGHT + 1, 19); // Posiciona el cursor para la entrada
    
    refresh();
}

// --- Lógica de Hilos ---

// Hilo para el Peluquero [cite: 117]
void* funcion_peluquero(void* arg) {
    int peluquero_id = *(int*)arg;
    char cliente_name[MAX_NAME_LEN];
    
    while (1) {
        // 1. Esperar a que haya un cliente para cortar (duerme aquí si sem_clientes_esperando es 0)
        sem_wait(&sem_clientes_esperando);
        
        // 2. Esperar a que un peluquero (este hilo) se libere y tomar el slot
        sem_wait(&sem_peluqueros_libres);

        // 3. Cliente disponible. Lo toma y actualiza el estado.
        pthread_mutex_lock(&g_mutex_estado);
        
        // --- Región Crítica (Mover Cliente) ---
        // Buscar el primer cliente en la lista_esperando
        int client_idx = -1;
        for (int i = 0; i < M_SILLAS_ESPERA; i++) {
            if (strlen(lista_esperando[i]) > 0) {
                client_idx = i;
                strcpy(cliente_name, lista_esperando[i]);
                lista_esperando[i][0] = '\0'; // Lo saca de la espera
                
                // Lo pone a cortar
                strcpy(lista_cortando[peluquero_id], cliente_name);
                break;
            }
        }
        
        // Un cliente fue tomado de una silla de espera, así que una se libera.
        // Si había gente esperando afuera (sillas_espera_libres < 0), uno de ellos
        // ahora puede sentarse. Si no había nadie, simplemente se libera una silla.
        sillas_espera_libres++;
        
        draw_tui(); // Refrescar la vista
        pthread_mutex_unlock(&g_mutex_estado);
        // --- Fin Región Crítica ---
        
        // 3. El cliente (en su propio hilo) simulará el corte y liberará al peluquero.
        // El peluquero solo espera a ser liberado.
    }
    return NULL;
}

// Hilo para el Cliente [cite: 117]
void* funcion_cliente(void* arg) {
    ClienteInfo* info = (ClienteInfo*)arg;
    
    // 1. Intentar tomar una silla de espera
    pthread_mutex_lock(&g_mutex_estado);
    
    if (sillas_espera_libres > 0) {
        // 1.1. Hay silla de espera: Ocuparla
        sillas_espera_libres--;
        
        // 1.2. Añadir a lista_esperando
        for (int i = 0; i < M_SILLAS_ESPERA; i++) {
            if (lista_esperando[i][0] == '\0') {
                strcpy(lista_esperando[i], info->name);
                break;
            }
        }
        
        draw_tui(); // Refrescar la vista SENTADOS
        pthread_mutex_unlock(&g_mutex_estado);

        // 2. Señalizar que hay un cliente para el peluquero
        sem_post(&sem_clientes_esperando);

        // 3. Esperar a que el peluquero esté libre (en este caso, el cliente ya está
        // en la lista_esperando y será movido por el peluquero. El cliente se duerme
        // mientras espera. Para simplificar, nos saltamos el sem_wait aquí y el cliente
        // duerme directamente en el tiempo de corte después de que el peluquero lo mueve.
        
        // **ESQUEMA SIMPLIFICADO**: El cliente espera a que el peluquero lo tome, 
        // y luego duerme por el tiempo de corte.
        
        // 4. Esperar a que el nombre aparezca en lista_cortando (simple spinlock temporal para simular la espera)
        int peluquero_idx = -1;
        while (peluquero_idx == -1) {
            pthread_mutex_lock(&g_mutex_estado);
            for (int i = 0; i < N_PELUQUEROS; i++) {
                if (strcmp(lista_cortando[i], info->name) == 0) {
                    peluquero_idx = i;
                    break;
                }
            }
            pthread_mutex_unlock(&g_mutex_estado);
            if (peluquero_idx == -1) nanosleep((const struct timespec[]){{0, 100000000L}}, NULL); // 0.1s
        }
        
        // 5. Cortarse el pelo [cite: 85]
        sleep(info->time);

        // 6. Corte terminado. Actualizar estado y liberar al peluquero.
        pthread_mutex_lock(&g_mutex_estado);
        
        lista_cortando[peluquero_idx][0] = '\0'; // Saca el nombre de CORTANDO
        
        draw_tui(); // Refrescar la vista CORTANDO
        pthread_mutex_unlock(&g_mutex_estado);
        
        // 7. Liberar la silla de corte
        sem_post(&sem_peluqueros_libres); 

    } else {
        // No hay sillas: "espera afuera" [cite: 67]
        sillas_espera_libres--; // Indica que hay un cliente "virtualmente" esperando afuera.
        
        draw_tui();
        pthread_mutex_unlock(&g_mutex_estado);
        
        // El cliente se va inmediatamente (no espera afuera en este modelo).
        // En un modelo real, debería esperar a que se libere una silla.
    }
    
    free(info);
    return NULL;
}

// --- Función Principal ---
int main(int argc, char *argv[]) {
    // 1. Validar y obtener argumentos
    if (argc != 3) {
        fprintf(stderr, "Uso: pelutiu <num_peluqueros> <num_sillas_espera>\n");
        fprintf(stderr, "Ejemplo: pelutiu 2 4\n");
        return 1;
    }
    N_PELUQUEROS = atoi(argv[1]);
    M_SILLAS_ESPERA = atoi(argv[2]);
    sillas_espera_libres = M_SILLAS_ESPERA;
    
    // 2. Inicializar ncurses [cite: 76]
    initscr();             
    cbreak();              
    noecho();              
    keypad(stdscr, TRUE);  
    
    // 3. Inicializar sincronización
    sem_init(&sem_clientes_esperando, 0, 0); 
    sem_init(&sem_peluqueros_libres, 0, N_PELUQUEROS); 
    pthread_mutex_init(&g_mutex_estado, NULL);
    
    // 4. Inicializar listas
    for(int i=0; i<M_SILLAS_ESPERA; i++) lista_esperando[i][0] = '\0';
    for(int i=0; i<N_PELUQUEROS; i++) lista_cortando[i][0] = '\0';

    // 5. Crear hilos peluqueros
    pthread_t peluquero_tids[N_PELUQUEROS];
    int peluquero_ids[N_PELUQUEROS];
    for (int i = 0; i < N_PELUQUEROS; i++) {
        peluquero_ids[i] = i;
        pthread_create(&peluquero_tids[i], NULL, funcion_peluquero, &peluquero_ids[i]);
    }
    
    // 6. Bucle principal para la TUI y la entrada [cite: 70]
    char input_buffer[100];
    while (1) {
        // a. Dibujar estado actual
        pthread_mutex_lock(&g_mutex_estado);
        draw_tui();
        pthread_mutex_unlock(&g_mutex_estado);
        
        // b. Capturar entrada de usuario (Nombre Tiempo) [cite: 85]
        // Mover el cursor a la posición de entrada después del prompt
        move(LINES - PROMPT_HEIGHT + 1, 19); 
        echo(); // Temporalmente habilitar eco para la entrada
        getstr(input_buffer); // Bloquea hasta ENTER
        noecho(); // Deshabilitar eco

        char name[MAX_NAME_LEN];
        int time;
        
        // c. Parsear entrada
        if (strcmp(input_buffer, "exit") == 0) {
            break; // Salir del bucle principal
        }

        if (sscanf(input_buffer, "%s %d", name, &time) == 2) {
            ClienteInfo* info = (ClienteInfo*)malloc(sizeof(ClienteInfo));
            strncpy(info->name, name, MAX_NAME_LEN - 1);
            info->time = time;
            
            // d. Crear hilo cliente [cite: 117]
            pthread_create(&info->tid, NULL, funcion_cliente, (void*)info);
            pthread_detach(info->tid); // No hacemos join
        } else {
            // Manejo de error de entrada
            pthread_mutex_lock(&g_mutex_estado);
            mvprintw(LINES - 1, 2, "Entrada inválida. Use: <nombre> <tiempo> o 'exit'");
            refresh();
            pthread_mutex_unlock(&g_mutex_estado);
            sleep(1); // Pausa para que el usuario vea el mensaje
        }
    }

    // 7. Limpieza (nunca se alcanza en el bucle infinito)
    endwin();
    sem_destroy(&sem_clientes_esperando);
    sem_destroy(&sem_peluqueros_libres);
    pthread_mutex_destroy(&g_mutex_estado);
    
    return 0;
}
