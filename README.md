# Driver de caracteres en Linux

Este repositorio contiene una clase demostrativa sobre un driver virtual de
caracteres. El dispositivo mantiene hasta 256 bytes en memoria y se presenta
como `/dev/sop_char`; no requiere hardware real.

## Materiales

- [`driver/`](driver/): driver completo con mutex, contador de aperturas y dos
  operaciones `ioctl`, más un cliente de prueba.

## Preparación rápida de la VM

Ubuntu 24.04 (recomendado para el laboratorio):

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
test -e /lib/modules/$(uname -r)/build/Makefile && echo "entorno listo"
```

Use una VM con snapshot y Secure Boot desactivado, o firme el módulo. No cargue
un módulo docente en un equipo de producción.

## Demostración rápida

```bash
cd driver
make
sudo insmod sop_char.ko
ls -l /dev/sop_char
printf 'Sistemas Operativos\n' | sudo tee /dev/sop_char >/dev/null
sudo cat /dev/sop_char
sudo ./test_sop_char
sudo rmmod sop_char
sudo dmesg | tail -n 20
```

`make` compila contra el kernel en ejecución. La compatibilidad de
`class_create()` se resuelve en el código para kernels anteriores y posteriores
a Linux 6.4.

## VS Code

Con la extensión Microsoft C/C++, ejecutar `make -C driver intellisense` desde
la raíz del proyecto para generar `driver/compile_commands.json` con los includes,
macros y opciones reales de Kbuild. La configuración en `.vscode/` utiliza esa
base para analizar el módulo; el cliente de usuario conserva los headers normales
del compilador. También está disponible la tarea «Actualizar IntelliSense del
kernel» en VS Code. Repetirla después de cambiar de kernel o de opciones de
compilación. Si el editor conserva errores anteriores, ejecutar «C/C++: Reset
IntelliSense Database» desde la paleta de comandos.

## Comportamiento y pruebas

El contenido se comparte entre aperturas, pero cada apertura tiene su propia
posición. Escribir desde la posición cero reemplaza el contenido; las escrituras
sucesivas continúan desde la posición actual. Una escritura que supera el espacio
disponible es parcial; con el buffer lleno devuelve `ENOSPC`. Escribir cero bytes
no modifica el contenido. Si falla la copia desde el usuario, el buffer se conserva.

`SOP_IOC_CLEAR` requiere un descriptor con permiso de escritura (en caso contrario
devuelve `EBADF`). Borra el contenido y reinicia la posición del descriptor que
ejecuta la operación; las otras aperturas conservan sus posiciones.

El cliente comprueba lectura parcial y EOF, capacidad máxima, varias aperturas,
permisos de borrado, escritura después de borrar y conservación del contenido ante
una copia fallida. Ejecutarlo modifica el contenido del dispositivo.
