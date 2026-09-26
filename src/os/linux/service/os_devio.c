#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int dev_open(const char *path, int oflag, int *fd)
{
    int os_fd = -1;
    int flag  = 0;
    int access_mode;
    int known_flags = MOFS_IO_OPEN_FLAG_RDWR | MOFS_IO_OPEN_FLAG_SYNC | MOFS_IO_OPEN_FLAG_DIRECT;

    if (fd == NULL) {
        return MOFS_EINVAL;
    }
    *fd = -1;

    if ((path == NULL) || ((oflag & ~known_flags) != 0)) {
        return MOFS_EINVAL;
    }

    access_mode = oflag & MOFS_IO_OPEN_FLAG_RDWR;
    if (access_mode == MOFS_IO_OPEN_FLAG_RDWR) {
        flag |= O_RDWR;
    } else if (access_mode == MOFS_IO_OPEN_FLAG_RDONLY) {
        flag |= O_RDONLY;
    } else if (access_mode == MOFS_IO_OPEN_FLAG_WRONLY) {
        flag |= O_WRONLY;
    } else {
        return MOFS_EINVAL;
    }

    if ((oflag & MOFS_IO_OPEN_FLAG_SYNC) != 0) {
        flag |= O_SYNC;
    }
    if ((oflag & MOFS_IO_OPEN_FLAG_DIRECT) != 0) {
        flag |= O_DIRECT;
    }

    os_fd = open(path, flag);
    if (os_fd < 0) {
        return os_to_mofs_errno(errno);
    }

    *fd = os_fd;
    return 0;
}

int dev_write(int fd, const void *buf, mofs_size_t count, mofs_size_t *written)
{
    ssize_t result;

    if (written == NULL) {
        return MOFS_EINVAL;
    }
    *written = 0U;
    if (buf == NULL) {
        return MOFS_EINVAL;
    }

    result = write(fd, buf, (size_t)count);
    if (result < 0) {
        return os_to_mofs_errno(errno);
    }

    *written = (mofs_size_t)result;
    return 0;
}

int dev_read(int fd, void *buf, mofs_size_t count, mofs_size_t *read_size)
{
    ssize_t result;

    if (read_size == NULL) {
        return MOFS_EINVAL;
    }
    *read_size = 0U;
    if (buf == NULL) {
        return MOFS_EINVAL;
    }

    result = read(fd, buf, (size_t)count);
    if (result < 0) {
        return os_to_mofs_errno(errno);
    }

    *read_size = (mofs_size_t)result;
    return 0;
}

int dev_fsync(int fd)
{
    if (fsync(fd) < 0) {
        return os_to_mofs_errno(errno);
    }
    return 0;
}

int dev_close(int fd)
{
    if (close(fd) < 0) {
        return os_to_mofs_errno(errno);
    }
    return 0;
}

int dev_lseek(int fd, mofs_off_t offset, int whence, mofs_off_t *new_offset)
{
    off_t result;
    int   os_whence;

    if (new_offset == NULL) {
        return MOFS_EINVAL;
    }
    *new_offset = 0;

    if (whence == MOFS_SEEK_SET) {
        os_whence = SEEK_SET;
    } else if (whence == MOFS_SEEK_CUR) {
        os_whence = SEEK_CUR;
    } else if (whence == MOFS_SEEK_END) {
        os_whence = SEEK_END;
    } else {
        return MOFS_EINVAL;
    }

    result = lseek(fd, (off_t)offset, os_whence);
    if (result < 0) {
        return os_to_mofs_errno(errno);
    }

    *new_offset = (mofs_off_t)result;
    return 0;
}

int dev_get_size(int fd, unsigned long long *size)
{
    struct stat st;

    if (size == NULL) {
        return MOFS_EINVAL;
    }
    *size = 0ULL;

    if (fstat(fd, &st) < 0) {
        return os_to_mofs_errno(errno);
    }

    if (S_ISBLK(st.st_mode)) {
        if (ioctl(fd, BLKGETSIZE64, size) < 0) {
            *size = 0ULL;
            return os_to_mofs_errno(errno);
        }
    } else if (S_ISREG(st.st_mode)) {
        *size = (unsigned long long)st.st_size;
    } else {
        return MOFS_EINVAL;
    }

    return 0;
}
