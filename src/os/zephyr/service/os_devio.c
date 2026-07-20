#include <mofs_devio.h>
#include <mofs_errno.h>

/* Zephyr stub: real disk_access backend comes in a later phase. */

int dev_open(const char *path, int oflag, int *fd)
{
    (void)path;
    (void)oflag;
    if (fd == NULL) {
        return MOFS_EINVAL;
    }
    *fd = -1;
    return MOFS_ENOSYS;
}

int dev_write(int fd, const void *buf, mofs_size_t count, mofs_size_t *written)
{
    (void)fd;
    (void)buf;
    (void)count;
    if (written == NULL) {
        return MOFS_EINVAL;
    }
    *written = 0U;
    return MOFS_ENOSYS;
}

int dev_read(int fd, void *buf, mofs_size_t count, mofs_size_t *read_size)
{
    (void)fd;
    (void)buf;
    (void)count;
    if (read_size == NULL) {
        return MOFS_EINVAL;
    }
    *read_size = 0U;
    return MOFS_ENOSYS;
}

int dev_fsync(int fd)
{
    (void)fd;
    return MOFS_ENOSYS;
}

int dev_close(int fd)
{
    (void)fd;
    return MOFS_ENOSYS;
}

int dev_lseek(int fd, mofs_off_t offset, int whence, mofs_off_t *new_offset)
{
    (void)fd;
    (void)offset;
    (void)whence;
    if (new_offset == NULL) {
        return MOFS_EINVAL;
    }
    *new_offset = 0;
    return MOFS_ENOSYS;
}

int dev_get_size(int fd, unsigned long long *size)
{
    (void)fd;
    if (size == NULL) {
        return MOFS_EINVAL;
    }
    *size = 0ULL;
    return MOFS_ENOSYS;
}
