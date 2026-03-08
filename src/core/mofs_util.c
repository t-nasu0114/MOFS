
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_struct.h>

static int read_one_block(int fd, void *buf)
{
    int ret = dev_read(fd, buf, MOFS_BLK_SIZE);

    if (ret != MOFS_BLK_SIZE) {
        ret = get_errno();
    } else {
        ret = 0;
    }
    return ret;
}

int read_continuous_blocks(int fd, void *buf, unsigned int blk_num)
{
    int ret = 0;

    if ((fd < 0) || (buf == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* Align check */
    if ((dev_lseek(fd, 0, MOFS_SEEK_CUR) % MOFS_BLK_SIZE) != 0) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        for (int i = 0; i < blk_num; i++) {
            ret = read_one_block(fd, buf);
            if (ret != 0)
                break;
            buf = (char *)buf + MOFS_BLK_SIZE;
        }
    }

    return ret;
}
