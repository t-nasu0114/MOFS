#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <mofs_devio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

int dev_open(const char *path, int oflag)
{
    int ret  = 0;
    int flag = 0;

    /* Flag set nothing */
    if ((oflag | MOFS_IO_OPEN_FLAG_NONE) == 0) {
        ret = EINVAL;
    }

    /* Check access mode flag */
    if (ret == 0) {
        if ((oflag & MOFS_IO_OPEN_FLAG_RDWR) == MOFS_IO_OPEN_FLAG_RDWR) {
            flag |= O_RDWR;
        } else if ((oflag & MOFS_IO_OPEN_FLAG_RDONLY) == MOFS_IO_OPEN_FLAG_RDONLY) {
            flag |= O_RDONLY;
        } else if ((oflag & MOFS_IO_OPEN_FLAG_WRONLY) == MOFS_IO_OPEN_FLAG_WRONLY) {
            flag |= O_WRONLY;
        } else {
            ret = EINVAL;
        }
    }

    if (ret == 0) {
        /* Check combination flag */
        if ((oflag & MOFS_IO_OPEN_FLAG_SYNC) == MOFS_IO_OPEN_FLAG_SYNC) {
            flag |= O_SYNC;
        }
        if ((oflag & MOFS_IO_OPEN_FLAG_DIRECT) == MOFS_IO_OPEN_FLAG_DIRECT) {
            flag |= O_DIRECT;
        }

        /* open */
        ret = open(path, flag);
    }

    return ret;
}

int dev_write(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

int dev_read(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

void dev_close(int fd)
{
    close(fd);
}

int dev_lseek(int fd, off_t offset, int whence)
{
    return lseek(fd, offset, whence);
}

unsigned long long dev_get_size(int fd, int *err)
{
    struct stat        st;
    unsigned long long bytes = 0;
    int                sta   = 0;

    if (fstat(fd, &st) < 0) {
        sta = errno;
    }

    if (sta == 0) {
        if (S_ISBLK(st.st_mode)) {
            sta = ioctl(fd, BLKGETSIZE64, &bytes);
            if (sta < 0) {
                bytes = 0;
                sta   = errno;
            }
        } else if (S_ISREG(st.st_mode)) {
            bytes = st.st_size;
        } else {
            sta = EINVAL;
        }
    }

    if (sta != 0) {
        *err = sta;
    }

    return bytes;
}
