#include <mofs_devio.h>
#include <mofs_errno.h>

/* Zephyr stub: real disk_access backend comes in a later phase. */

int dev_open(const char *path, int oflag)
{
    (void)path;
    (void)oflag;
    return -1;
}

int dev_write(int fd, const void *buf, mofs_size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

int dev_read(int fd, void *buf, mofs_size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

int dev_fsync(int fd)
{
    (void)fd;
    return -1;
}

void dev_close(int fd)
{
    (void)fd;
}

mofs_off_t dev_lseek(int fd, mofs_off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    return (mofs_off_t)-1;
}

unsigned long long dev_get_size(int fd, int *err)
{
    (void)fd;
    if (err != NULL) {
        *err = MOFS_ENOSYS;
    }
    return 0ULL;
}
