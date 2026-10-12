#ifndef SOP_CHAR_IOCTL_H
#define SOP_CHAR_IOCTL_H

#include <linux/ioctl.h>

#define SOP_IOC_MAGIC 's'
#define SOP_IOC_GET_LEN _IOR(SOP_IOC_MAGIC, 1, unsigned int)
#define SOP_IOC_CLEAR _IO(SOP_IOC_MAGIC, 2)
#define SOP_IOC_GET_OPEN_COUNT _IOR(SOP_IOC_MAGIC, 3, unsigned int)

#endif
