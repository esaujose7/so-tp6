#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

// --- Definiciones del Buffer y Rutas ---
#define BUFFER_SIZE 5
#define FILE_MAX_SIZE (48 * 1024) // 48 KiB [cite: 60]

// Estructura para almacenar la información del archivo en el buffer
typedef struct {
    char filename[256];
    char content[FILE_MAX_SIZE];
    long size;
} FileData;

// --- Variables Globales Compartidas ---
FileData shared_buffer[BUFFER_SIZE];
int buffer_in = 0;
int buffer_out = 0;
char *g_origen_path;
char *g_destino_path;

// --- Mecanismos de Sincronización [cite: 57] ---
sem_t sem_full;  // Cuenta espacios llenos (inicial: 0)
sem_t sem_empty; // Cuenta espacios vacíos (inicial: 5)
pthread_mutex_t buffer_mutex; // Mutex para el acceso a buffer_in/out y al buffer

// --- Documentación de Funcionamiento [cite: 34] ---
void print_usage() {
    fprintf(stderr, "Uso: toupperd <carpeta_origen> <carpeta_destino>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "toupperd: Servicio para convertir archivos a mayúsculas.\n");
    fprintf(stderr, "Verifica si se han colocado uno o más archivos en <carpeta_origen>,\n");
    fprintf(stderr, "los procesa y genera un nuevo archivo en <carpeta_destino> con\n");
    fprintf(stderr, "todos los caracteres pasados a mayúscula. Los archivos originales\n");
    fprintf(stderr, "son eliminados tras el procesamiento.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Requerimientos: Implementado con hilos sincronizados (Productor/Consumidor).\n");
}

// --- Hilo Productor: Escanea y Carga Archivos [cite: 61, 62] ---
void* hilo_productor(void* arg) {
    while (1) {
        DIR *dirp;
        struct dirent *entry;
        
        // 1. Abrir y escanear el directorio origen
        if ((dirp = opendir(g_origen_path)) == NULL) {
            fprintf(stderr, "Productor: Error abriendo directorio origen %s\n", g_origen_path);
            sleep(5);
            continue;
        }

        while ((entry = readdir(dirp)) != NULL) {
            // Ignorar . y ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char full_path_origen[512];
            snprintf(full_path_origen, sizeof(full_path_origen), "%s/%s", g_origen_path, entry->d_name);

            // 2. Intentar leer el archivo
            FILE *f_in = fopen(full_path_origen, "rb");
            if (f_in == NULL) {
                // Podría estar siendo escrito por otro proceso, ignorar por ahora
                continue;
            }
            
            // Leer el contenido
            long bytes_read;
            
            // 3. Esperar a que haya un espacio vacío en el buffer [cite: 58]
            sem_wait(&sem_empty);

            // 4. Bloquear la región crítica del buffer
            pthread_mutex_lock(&buffer_mutex);

            // --- Región Crítica (Escribir en Buffer) ---
            bytes_read = fread(shared_buffer[buffer_in].content, 1, FILE_MAX_SIZE, f_in);
            shared_buffer[buffer_in].size = bytes_read;
            strncpy(shared_buffer[buffer_in].filename, entry->d_name, sizeof(shared_buffer[buffer_in].filename) - 1);
            
            buffer_in = (buffer_in + 1) % BUFFER_SIZE;
            // --- Fin Región Crítica ---

            // 5. Desbloquear la región crítica
            pthread_mutex_unlock(&buffer_mutex);

            // 6. Señalizar que hay un espacio lleno
            sem_post(&sem_full);
            
            printf("Productor: Archivo '%s' cargado al buffer. (Tamaño: %ld)\n", entry->d_name, bytes_read);
            
            // Cierra el archivo de entrada después de cargarlo al buffer
            fclose(f_in);
            break; // Solo procesa un archivo por escaneo para evitar monopolizar
        }
        
        closedir(dirp);
        sleep(2); // Espera antes de volver a escanear [cite: 46]
    }
    return NULL;
}

// --- Hilo Consumidor: Procesa, Guarda y Elimina Archivos [cite: 63] ---
void* hilo_consumidor(void* arg) {
    while (1) {
        // 1. Esperar a que haya un espacio lleno
        sem_wait(&sem_full);

        // 2. Bloquear la región crítica del buffer
        pthread_mutex_lock(&buffer_mutex);

        // --- Región Crítica (Leer de Buffer) ---
        // Copia local para liberar rápido el mutex
        char current_filename[256];
        char current_content[FILE_MAX_SIZE];
        long current_size;

        current_size = shared_buffer[buffer_out].size;
        strncpy(current_filename, shared_buffer[buffer_out].filename, sizeof(current_filename) - 1);
        memcpy(current_content, shared_buffer[buffer_out].content, current_size);
        
        buffer_out = (buffer_out + 1) % BUFFER_SIZE;
        // --- Fin Región Crítica ---

        // 3. Desbloquear la región crítica
        pthread_mutex_unlock(&buffer_mutex);

        // 4. Señalizar que hay un espacio vacío
        sem_post(&sem_empty);

        // --- Procesamiento (Fuera de la Región Crítica) ---
        // Ahora el procesamiento se hace sobre la copia local
        for (long i = 0; i < current_size; i++) {
            current_content[i] = toupper(current_content[i]);
        }

        char full_path_destino[512];
        snprintf(full_path_destino, sizeof(full_path_destino), "%s/%s", g_destino_path, current_filename);
        
        // 5. Escribir el archivo en destino
        FILE *f_out = fopen(full_path_destino, "wb");
        if (f_out) {
            fwrite(current_content, 1, current_size, f_out);
            fclose(f_out);
            printf("Consumidor: Archivo '%s' procesado y guardado en destino.\n", current_filename);
        } else {
            fprintf(stderr, "Consumidor: Error al escribir en destino %s\n", full_path_destino); 
        }
        
        // 6. Eliminar el archivo original [cite: 63]
        char full_path_origen[512];
        snprintf(full_path_origen, sizeof(full_path_origen), "%s/%s", g_origen_path, current_filename);
        if (remove(full_path_origen) != 0) {
            fprintf(stderr, "Consumidor: Error al eliminar el archivo original %s\n", full_path_origen);
        } else {
            printf("Consumidor: Archivo original '%s' eliminado.\n", current_filename);
        }
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_usage();
        return 1;
    }
    
    g_origen_path = argv[1];
    g_destino_path = argv[2];

    // 1. Inicializar mecanismos de sincronización
    sem_init(&sem_full, 0, 0); // 0 elementos llenos
    sem_init(&sem_empty, 0, BUFFER_SIZE); // 5 elementos vacíos
    pthread_mutex_init(&buffer_mutex, NULL);
    
    pthread_t productor_tid, consumidor_tid;
    
    // 2. Crear los hilos
    if (pthread_create(&productor_tid, NULL, hilo_productor, NULL) != 0) {
        perror("Error creando hilo productor");
        return 1;
    }
    
    if (pthread_create(&consumidor_tid, NULL, hilo_consumidor, NULL) != 0) {
        perror("Error creando hilo consumidor");
        return 1;
    }
    
    printf("toupperd iniciado. Origen: %s, Destino: %s\n", g_origen_path, g_destino_path);
    printf("Presiona Ctrl+C para detener el servicio.\n");

    // 3. Esperar indefinidamente
    pthread_join(productor_tid, NULL);
    pthread_join(consumidor_tid, NULL);

    // 4. Limpieza (nunca se alcanzará en un daemon/servicio, pero es buena práctica)
    sem_destroy(&sem_full);
    sem_destroy(&sem_empty);
    pthread_mutex_destroy(&buffer_mutex);

    return 0;
}
