#ifndef __MOFS_DEVIO__
#define __MOFS_DEVIO__

#include <mofs_port_types.h>

/* Original Open Flags */
#define MOFS_IO_OPEN_FLAG_NONE   0
#define MOFS_IO_OPEN_FLAG_RDONLY 0b0001
#define MOFS_IO_OPEN_FLAG_WRONLY 0b0010
#define MOFS_IO_OPEN_FLAG_RDWR   (MOFS_IO_OPEN_FLAG_RDONLY | MOFS_IO_OPEN_FLAG_WRONLY)
#define MOFS_IO_OPEN_FLAG_SYNC   0b0100
#define MOFS_IO_OPEN_FLAG_DIRECT 0b1000

int  dev_open(const char *path, int oflag);
int  dev_write(int fd, const void *buf, mofs_size_t count);
int  dev_read(int fd, void *buf, mofs_size_t count);
void dev_close(int fd);

/* Original whence Flags */
#define MOFS_SEEK_SET 0
#define MOFS_SEEK_CUR 1
#define MOFS_SEEK_END 2

mofs_off_t dev_lseek(int fd, mofs_off_t offset, int whence);

/* Get device size */
unsigned long long dev_get_size(int fd, int *err);

#endif /* __MOFS_DEVIO__ */
