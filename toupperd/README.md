# toupperd

`toupperd` es un servicio que convierte archivos de texto a mayúsculas. Monitorea un directorio de entrada, y cuando un archivo es depositado allí, lo procesa y escribe una versión en mayúsculas en un directorio de salida. El archivo original es eliminado.

## Compilación

Para compilar el programa, simplemente ejecuta `make`:

```bash
make
```

Esto generará un ejecutable llamado `toupperd`.

## Uso

Para correr el servicio, ejecuta el programa con dos argumentos: el directorio de origen y el directorio de destino.

```bash
./toupperd <directorio_origen> <directorio_destino>
```

Por ejemplo:

```bash
./toupperd in out
```

El programa quedará corriendo, esperando por archivos en el directorio `in`. Cuando un archivo aparezca en `in`, será procesado, movido a `out` con su contenido en mayúsculas, y el original será eliminado.

Para detener el servicio, presiona `Ctrl+C`.
