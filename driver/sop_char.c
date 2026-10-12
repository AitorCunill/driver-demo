#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "sop_char_ioctl.h"

#define SOP_NAME "sop_char"
#define SOP_CLASS "sop"
#define SOP_BUFFER_SIZE 256

/* Identidad major/minor y objetos que conectan el dispositivo con el kernel. */
static dev_t sop_dev;
static struct cdev sop_cdev;
static struct class *sop_class;
static struct device *sop_device;
/* Todas las aperturas comparten los datos; el mutex protege buffer y longitud. */
static char sop_buffer[SOP_BUFFER_SIZE];
static size_t data_len;
static DEFINE_MUTEX(buffer_lock);
/* Contador acumulado, no cantidad de descriptores actualmente abiertos. */
static atomic_t total_opens = ATOMIC_INIT(0);

static int sop_open(struct inode *inode, struct file *file)
{
	/* La operación atómica evita perder incrementos entre aperturas simultáneas. */
	int opens = atomic_inc_return(&total_opens);

	pr_info(SOP_NAME ": open (aperturas acumuladas=%d)\n", opens);
	return 0;
}

/* Se invoca al liberar la última referencia a esta apertura (struct file). */
static int sop_release(struct inode *inode, struct file *file)
{
	pr_debug(SOP_NAME ": release\n");
	return 0;
}

/*
 * position indica desde dónde leer: cada apertura independiente tiene su offset.
 * __user identifica memoria de usuario, que debe accederse mediante uaccess.
 */
static ssize_t sop_read(struct file *file, char __user *user_buffer,
			size_t count, loff_t *position)
{
	size_t available;
	size_t amount;

	/* La espera puede interrumpirse por una señal; el kernel gestiona el reinicio. */
	if (mutex_lock_interruptible(&buffer_lock))
		return -ERESTARTSYS;

	/* Retornar cero al alcanzar el final permite que herramientas como cat terminen. */
	if (*position < 0 || (size_t)*position >= data_len) {
		mutex_unlock(&buffer_lock);
		return 0;
	}

	available = data_len - (size_t)*position;
	amount = min(count, available);
	/* copy_to_user devuelve bytes NO copiados; un valor distinto de cero es fallo. */
	if (copy_to_user(user_buffer, sop_buffer + *position, amount)) {
		mutex_unlock(&buffer_lock);
		return -EFAULT;
	}

	*position += amount;
	mutex_unlock(&buffer_lock);
	return amount;
}

/* Acepta como máximo el espacio restante y devuelve la cantidad escrita. */
static ssize_t sop_write(struct file *file, const char __user *user_buffer,
				 size_t count, loff_t *position)
{
	/* Evita modificar el buffer compartido si la copia de usuario falla a mitad. */
	char temporary[SOP_BUFFER_SIZE];
	size_t start;
	size_t amount;

	/* Una escritura vacía no debe truncar el contenido ni fallar por buffer lleno. */
	if (!count)
		return 0;

	if (mutex_lock_interruptible(&buffer_lock))
		return -ERESTARTSYS;

	if (*position < 0 || (size_t)*position >= SOP_BUFFER_SIZE) {
		mutex_unlock(&buffer_lock);
		return -ENOSPC;
	}

	start = (size_t)*position;
	amount = min(count, (size_t)SOP_BUFFER_SIZE - start);
	if (copy_from_user(temporary, user_buffer, amount)) {
		mutex_unlock(&buffer_lock);
		return -EFAULT;
	}

	memcpy(sop_buffer + start, temporary, amount);
	*position += amount;
	/* Escribir desde cero reemplaza los datos; desde otro offset conserva el resto. */
	data_len = start == 0 ? amount : max(data_len, (size_t)*position);
	mutex_unlock(&buffer_lock);
	return amount;
}

/* Operaciones de control definidas en la interfaz compartida con el cliente. */
static long sop_ioctl(struct file *file, unsigned int command,
		      unsigned long argument)
{
	unsigned int length;

	/* Rechaza comandos que no pertenecen a esta interfaz. */
	if (_IOC_TYPE(command) != SOP_IOC_MAGIC)
		return -ENOTTY;

	switch (command) {
	case SOP_IOC_GET_LEN:
		/* Captura la longitud bajo el mutex y luego entrega esa instantánea. */
		if (mutex_lock_interruptible(&buffer_lock))
			return -ERESTARTSYS;
		length = data_len;
		mutex_unlock(&buffer_lock);
		if (copy_to_user((unsigned int __user *)argument, &length,
				 sizeof(length)))
			return -EFAULT;
		return 0;

	case SOP_IOC_CLEAR:
		/* ioctl debe comprobar explícitamente el permiso para modificar los datos. */
		if (!(file->f_mode & FMODE_WRITE))
			return -EBADF;
		if (mutex_lock_interruptible(&buffer_lock))
			return -ERESTARTSYS;
		memset(sop_buffer, 0, sizeof(sop_buffer));
		data_len = 0;
		/* Otras aperturas independientes conservan su posición. */
		file->f_pos = 0;
		mutex_unlock(&buffer_lock);
		return 0;

	default:
		return -ENOTTY;
	}
}

/* El VFS utiliza esta tabla para despachar las operaciones al driver. */
static const struct file_operations sop_fops = {
	/* Mantiene el módulo referenciado mientras se utiliza esta tabla. */
	.owner = THIS_MODULE,
	.open = sop_open,
	.release = sop_release,
	.read = sop_read,
	.write = sop_write,
	.unlocked_ioctl = sop_ioctl,
	.llseek = noop_llseek, /* lseek devuelve la posición actual sin cambiarla. */
};

/* __init permite liberar la memoria de esta función después de la carga. */
static int __init sop_init(void)
{
	int ret;

	/* Reserva dinámicamente un major y un único minor, empezando en cero. */
	ret = alloc_chrdev_region(&sop_dev, 0, 1, SOP_NAME);
	if (ret) {
		pr_err(SOP_NAME ": alloc_chrdev_region falló: %d\n", ret);
		return ret;
	}

	/* Asocia las operaciones y publica el rango de números en el subsistema cdev. */
	cdev_init(&sop_cdev, &sop_fops);
	sop_cdev.owner = THIS_MODULE;
	ret = cdev_add(&sop_cdev, sop_dev, 1);
	if (ret)
		goto err_unregister;

	/* La firma de class_create cambió en Linux 6.4. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	sop_class = class_create(SOP_CLASS);
#else
	sop_class = class_create(THIS_MODULE, SOP_CLASS);
#endif
	if (IS_ERR(sop_class)) {
		ret = PTR_ERR(sop_class);
		goto err_cdev;
	}

	/* Registra el dispositivo en sysfs; devtmpfs/udev permite crear el nodo /dev. */
	sop_device = device_create(sop_class, NULL, sop_dev, NULL, SOP_NAME);
	if (IS_ERR(sop_device)) {
		ret = PTR_ERR(sop_device);
		goto err_class;
	}

	pr_info(SOP_NAME ": major=%d minor=%d buffer=%d\n",
		MAJOR(sop_dev), MINOR(sop_dev), SOP_BUFFER_SIZE);
	return 0;

/* Deshace en orden inverso únicamente los pasos que llegaron a completarse. */
err_class:
	class_destroy(sop_class);
err_cdev:
	cdev_del(&sop_cdev);
err_unregister:
	unregister_chrdev_region(sop_dev, 1);
	pr_err(SOP_NAME ": no se pudo inicializar: %d\n", ret);
	return ret;
}

/* Al descargar el módulo, retira los objetos y libera los números reservados. */
static void __exit sop_exit(void)
{
	device_destroy(sop_class, sop_dev);
	class_destroy(sop_class);
	cdev_del(&sop_cdev);
	unregister_chrdev_region(sop_dev, 1);
	pr_info(SOP_NAME ": descargado; aperturas acumuladas=%d\n",
		atomic_read(&total_opens));
}

/* Declara los puntos de entrada y salida que invoca el cargador de módulos. */
module_init(sop_init);
module_exit(sop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cátedra de Sistemas Operativos");
MODULE_DESCRIPTION("Driver virtual de caracteres con ioctl para docencia");
MODULE_VERSION("1.0");
