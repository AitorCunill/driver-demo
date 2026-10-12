#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sop_char_ioctl.h"

#define DEVICE "/dev/sop_char"
#define CAPACITY 256

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "FALLO línea %d: %s (errno=%d)\n", \
			__LINE__, #condition, errno); \
		goto cleanup; \
	} \
} while (0)

int main(void)
{
	char payload[CAPACITY + 1];
	char buffer[CAPACITY];
	unsigned int length;
	unsigned int open_count;
	int count_fd1 = -1, count_fd2 = -1;
	int writer = -1, reader = -1, other = -1;
	int status = 1;
	long page_size = sysconf(_SC_PAGESIZE);
	char *mapping = MAP_FAILED;

	memset(payload, 'A', sizeof(payload));
	CHECK(page_size >= CAPACITY);
	    /* Prueba del contador de aperturas activas. */
    count_fd1 = open(DEVICE, O_RDONLY);
    CHECK(count_fd1 >= 0);

    CHECK(ioctl(count_fd1, SOP_IOC_GET_OPEN_COUNT, &open_count) == 0);
    CHECK(open_count == 1);

    count_fd2 = open(DEVICE, O_RDONLY);
    CHECK(count_fd2 >= 0);

    CHECK(ioctl(count_fd1, SOP_IOC_GET_OPEN_COUNT, &open_count) == 0);
    CHECK(open_count == 2);

    CHECK(close(count_fd2) == 0);
    count_fd2 = -1;

    CHECK(ioctl(count_fd1, SOP_IOC_GET_OPEN_COUNT, &open_count) == 0);
    CHECK(open_count == 1);

    CHECK(ioctl(count_fd1, SOP_IOC_GET_OPEN_COUNT, &open_count) == 0);
    CHECK(open_count == 1);

    CHECK(close(count_fd1) == 0);
    count_fd1 = -1;

    count_fd1 = open(DEVICE, O_RDONLY);
    CHECK(count_fd1 >= 0);

    CHECK(ioctl(count_fd1, SOP_IOC_GET_OPEN_COUNT, &open_count) == 0);
    CHECK(open_count == 1);

    CHECK(close(count_fd1) == 0);
    count_fd1 = -1;

    puts("OK: contador de aperturas activas (1, 2, 1, 1, 1)");

	writer = open(DEVICE, O_RDWR);
	CHECK(writer >= 0);
	CHECK(ioctl(writer, SOP_IOC_CLEAR) == 0);
	CHECK(write(writer, payload, sizeof(payload)) == CAPACITY);
	CHECK(write(writer, payload, 1) == -1 && errno == ENOSPC);
	CHECK(write(writer, payload, 0) == 0);
	CHECK(ioctl(writer, SOP_IOC_GET_LEN, &length) == 0);
	CHECK(length == CAPACITY);

	reader = open(DEVICE, O_RDONLY);
	CHECK(reader >= 0);
	CHECK(ioctl(reader, SOP_IOC_CLEAR) == -1 && errno == EBADF);
	CHECK(ioctl(reader, SOP_IOC_GET_LEN, &length) == 0);
	CHECK(length == CAPACITY);
	CHECK(read(reader, buffer, 17) == 17);
	CHECK(read(reader, buffer + 17, sizeof(buffer) - 17) == CAPACITY - 17);
	CHECK(memcmp(buffer, payload, CAPACITY) == 0);
	CHECK(read(reader, buffer, 1) == 0);

	/* Una nueva apertura comienza en cero y las escrituras vacías no borran. */
	other = open(DEVICE, O_RDWR);
	CHECK(other >= 0);
	CHECK(write(other, payload, 0) == 0);
	CHECK(ioctl(other, SOP_IOC_GET_LEN, &length) == 0);
	CHECK(length == CAPACITY);

	/* La copia cruza desde una página válida a una inaccesible. */
	mapping = mmap(NULL, (size_t)page_size * 2, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(mapping != MAP_FAILED);
	memset(mapping, 'B', (size_t)page_size);
	CHECK(mprotect(mapping + page_size, (size_t)page_size, PROT_NONE) == 0);
	CHECK(write(other, mapping + page_size - 128, CAPACITY) == -1 &&
	      errno == EFAULT);
	CHECK(ioctl(other, SOP_IOC_GET_LEN, &length) == 0);
	CHECK(length == CAPACITY);
	CHECK(read(other, buffer, sizeof(buffer)) == CAPACITY);
	CHECK(memcmp(buffer, payload, CAPACITY) == 0);

	/* CLEAR reinicia la posición del descriptor que lo ejecuta. */
	CHECK(ioctl(writer, SOP_IOC_CLEAR) == 0);
	CHECK(ioctl(writer, SOP_IOC_GET_LEN, &length) == 0);
	CHECK(length == 0);
	CHECK(write(writer, "hola", 4) == 4);
	CHECK(write(writer, "!", 1) == 1);
	CHECK(ioctl(writer, SOP_IOC_GET_LEN, &length) == 0);
	CHECK(length == 5);
	/* La posición de otra apertura permanece al final del contenido anterior. */
	CHECK(read(reader, buffer, sizeof(buffer)) == 0);
	close(reader);
	reader = open(DEVICE, O_RDONLY);
	CHECK(reader >= 0);
	CHECK(read(reader, buffer, sizeof(buffer)) == 5);
	CHECK(memcmp(buffer, "hola!", 5) == 0);
	CHECK(read(reader, buffer, 1) == 0);
	CHECK(ioctl(writer, SOP_IOC_CLEAR) == 0);
	puts("OK: capacidad, lecturas parciales, EOF, aperturas, permisos, CLEAR y EFAULT");
	status = 0;

cleanup:
    if (mapping != MAP_FAILED)
        munmap(mapping, (size_t)page_size * 2);

    if (count_fd2 >= 0)
        close(count_fd2);

    if (count_fd1 >= 0)
        close(count_fd1);

    if (other >= 0)
        close(other);

    if (reader >= 0)
        close(reader);

    if (writer >= 0)
        close(writer);

    return status;
}
