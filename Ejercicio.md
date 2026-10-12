# Ejercicio: aperturas activas de un driver de caracteres

## Objetivo

Modificar el driver `/dev/sop_char`, con ayuda de una IA, para consultar cuántas
aperturas del dispositivo permanecen activas. No se requieren conocimientos previos
de programación en C: el objetivo es comprender el comportamiento del driver,
orientar a la IA y verificar el resultado.

## Consigna

1. Agregar un contador de aperturas activas que comience en cero, aumente con cada
   apertura exitosa y disminuya cuando se libere esa apertura. Conservar el contador
   acumulado existente: ambos representan información diferente.
2. Usar operaciones atómicas para actualizar y consultar el nuevo contador.
3. Definir en `driver/sop_char_ioctl.h` un comando llamado
   `SOP_IOC_GET_OPEN_COUNT`, con un número que no esté utilizado. Debe devolver un
   valor de tipo `unsigned int` al programa de usuario.
4. Implementar el comando en `sop_ioctl()`. La consulta no debe modificar el
   contador y debe utilizar el mecanismo de copia adecuado para entregar el valor
   a la memoria del proceso.
5. Ampliar `driver/test_sop_char.c` para comprobar el nuevo comportamiento,
   conservando las pruebas existentes. No modificar el funcionamiento de lectura,
   escritura ni de los otros comandos `ioctl`.

**Qué estamos contando:** aperturas activas, representadas por `struct file`, no
todos los descriptores que puedan referirse a ellas. Cada llamada independiente a
`open()` crea una apertura. El callback `release` se ejecuta cuando se libera su
última referencia.

## Trabajo con IA

Compartir con la IA el código del driver, la interfaz de `ioctl` y el programa de
prueba. Pedirle que explique los cambios antes de aplicarlos y que ayude a
interpretar los errores de compilación o los resultados inesperados.

Un posible pedido inicial:

> Estoy estudiando Sistemas Operativos y no sé programar en C. Necesito agregar
> un contador de aperturas activas y un ioctl para consultarlo en este driver.
> Ayudame a identificar qué funciones debo modificar, explicá cómo se diferencia
> este contador del acumulado y proponé cómo verificar su funcionamiento.

No alcanza con obtener código que compile: deberán comprobar que hace lo pedido
y explicar por qué los resultados son correctos.

## Pruebas obligatorias

Ejecutar las pruebas en la VM del laboratorio, sin otros programas que mantengan
abierto el dispositivo. La prueba del contador debe comenzar sin descriptores de
pruebas anteriores abiertos.

| Paso | Acción | Valor esperado |
|---|---|---:|
| 1 | Abrir el dispositivo y consultar desde ese descriptor | 1 |
| 2 | Abrirlo por segunda vez, sin cerrar la primera apertura, y consultar | 2 |
| 3 | Cerrar la segunda apertura y consultar desde la primera | 1 |
| 4 | Repetir la consulta sin abrir ni cerrar nada | 1 |
| 5 | Cerrar la primera apertura, volver a abrir y consultar | 1 |

La consulta incluye la apertura del propio programa que la realiza. Por eso no
se espera obtener cero mediante un `ioctl` ejecutado desde una apertura válida.
El valor es una instantánea: otros procesos podrían cambiarlo después de leerlo.

Compilar con `make` dentro de `driver/`. Si hay una versión anterior del módulo
cargada, cerrar los programas que lo usan, descargarla y cargar la versión nueva
antes de ejecutar el cliente:

```bash
cd driver
make
sudo rmmod sop_char       # Solo si el módulo ya estaba cargado.
sudo insmod sop_char.ko
sudo ./test_sop_char
sudo rmmod sop_char
```

Verificar también que las pruebas anteriores del dispositivo siguen pasando.

## Entrega

- Los tres archivos modificados: `sop_char.c`, `sop_char_ioctl.h` y
  `test_sop_char.c`.
- La salida de compilación y de las pruebas, con los valores observados del
  contador en cada paso.
- Una explicación breve que responda:
  1. ¿En qué funciones se incrementa y decrementa el contador? ¿Por qué?
  2. ¿Qué diferencia hay entre aperturas acumuladas y aperturas activas?
  3. ¿Por qué se utilizan operaciones atómicas?
  4. ¿Cómo recibe el programa de usuario el resultado del `ioctl`?
  5. ¿Por qué la primera consulta devuelve uno y no cero?
- Un ejemplo de un pedido realizado a la IA y cómo verificaron su respuesta.
  Si tuvieron que corregir alguna sugerencia, explicar cuál y por qué.

Se evaluará el comportamiento observado, la conservación de las funciones
existentes y la explicación de la solución, no la memorización de sintaxis de C.

## Desafío opcional: descriptores y aperturas

Duplicar un descriptor mediante `dup()` y consultar el contador. Luego cerrar el
descriptor original y consultar desde el duplicado.

¿Por qué duplicar el descriptor no aumenta el contador? ¿Por qué cerrar el
original no lo disminuye mientras el duplicado continúa abierto? Relacionar la
respuesta con `struct file` y con el momento en que se ejecuta `release`.
