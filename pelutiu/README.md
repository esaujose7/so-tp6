# Peluquería Interactiva - Simulación del Barbero Durmiente

Este programa es una simulación interactiva del clásico problema de concurrencia "El Barbero Durmiente", implementado en C con `pthreads` para el manejo de hilos y `ncurses` para la interfaz de usuario en la terminal.

## Descripción del Funcionamiento

El programa simula una peluquería con un número configurable de peluqueros y sillas de espera.

-   **Peluqueros:** Son hilos que esperan a que lleguen clientes. Si no hay clientes, el peluquero "duerme". Cuando un cliente llega, el peluquero le corta el pelo (simulado con un `sleep`) y luego busca al siguiente cliente.
-   **Clientes:** Son hilos que se crean dinámicamente. Al llegar, un cliente busca una silla libre en la sala de espera.
    -   Si hay sillas libres, el cliente se sienta y espera a que un peluquero se desocupe.
    -   Si no hay sillas libres, el cliente se queda "esperando afuera".
-   **Interfaz (TUI):** La pantalla muestra en tiempo real el estado de la peluquería:
    -   **ESPERANDO (Afuera):** Clientes que llegaron y no encontraron sillas.
    -   **SENTADOS:** Clientes que ocupan las sillas de la sala de espera.
    -   **CORTANDO:** Clientes que están siendo atendidos por un peluquero.

## Compilación

Puedes compilar el programa de dos maneras:

### 1. Manualmente

Usa el siguiente comando para compilar el programa, enlazando las librerías `ncurses` y `pthread`:

```bash
gcc -Wall -Wextra pelutiu.c -o pelutiu -lncurses -lpthread
```

### 2. Usando Makefile

El repositorio incluye un `Makefile` que simplifica el proceso.

-   **Para compilar:**
    ```bash
    make
    ```
-   **Para limpiar los archivos compilados:**
    ```bash
    make clean
    ```

## Ejecución

Para correr el programa, debes ejecutar el archivo `pelutiu` y pasarle dos argumentos numéricos:

1.  El número de peluqueros.
2.  El número de sillas en la sala de espera.

**Formato:**
```bash
./pelutiu <num_peluqueros> <num_sillas_espera>
```

**Ejemplo:**
Para iniciar la simulación con 2 peluqueros y 4 sillas de espera:
```bash
./pelutiu 2 4
```

### Interacción

Una vez que el programa está corriendo:
-   **Para añadir un cliente:** Escribe el nombre del cliente y el tiempo que durará su corte, luego presiona `Enter`.
    -   **Ejemplo:** `Juan 5`
-   **Para salir del programa:** Escribe `exit` y presiona `Enter`.