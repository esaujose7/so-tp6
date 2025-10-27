# Proyectos del Repositorio

Este repositorio contiene dos proyectos principales:

1.  `pelutiu`: Una simulación interactiva del problema del Barbero Durmiente.
2.  `toupperd`: Un servicio que convierte archivos de texto a mayúsculas.

## Cómo Ejecutar los Proyectos desde la Raíz

### 1. `pelutiu` (Simulación del Barbero Durmiente)

Este proyecto simula una peluquería con barberos y clientes usando `pthreads` y una interfaz `ncurses`.

**Compilación:**

```bash
cd pelutiu
make
cd ..
```

**Ejecución:**

```bash
./pelutiu/pelutiu <num_peluqueros> <num_sillas_espera>
```

Ejemplo:
```bash
./pelutiu/pelutiu 2 4
```

### 2. `toupperd` (Servicio de Conversión a Mayúsculas)

Este proyecto es un servicio que monitorea un directorio de origen, convierte los archivos a mayúsculas y los guarda en un directorio de destino.

**Compilación:**

```bash
cd toupperd
make
cd ..
```

**Ejecución:**

```bash
./toupperd/toupperd <directorio_origen> <directorio_destino>
```

Ejemplo:
```bash
./toupperd/toupperd toupperd/in toupperd/out
```

**Nota:** Asegúrate de crear los directorios `in` y `out` dentro de `toupperd/` antes de ejecutar el servicio, si no existen.
