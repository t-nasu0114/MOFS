
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <stddef.h>

static int read_one_block(int fd, void *buf, int *err)
{
    int ret = dev_read(fd, buf, MOFS_BLK_SIZE);

    if (ret == -1) {
        *err = get_errno();
        ret  = 0;
    } else {
        *err = 0;
    }

    return ret;
}

int read_continuous_blocks(int fd, void *buf, unsigned int blk_num, unsigned int *read_blk_num, size_t *fraction)
{
    int err = 0;
    int ret = 0;

    if ((fd < 0) || (buf == NULL) || (read_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* Align check */
    if ((dev_lseek(fd, 0, MOFS_SEEK_CUR) % MOFS_BLK_SIZE) != 0) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        *fraction     = 0U;
        *read_blk_num = 0U;

        for (int i = 0; i < blk_num; i++) {
            ret = read_one_block(fd, buf, &err);
            if (ret == 0) {
                ret = err;
                break;
            } else if (ret != MOFS_BLK_SIZE) {
                *fraction = ret;
                ret       = 0;
                break;
            } else {
                ret = 0;
            }
            *read_blk_num = *read_blk_num + 1;
            buf           = (char *)buf + MOFS_BLK_SIZE;
        }
    }

    return ret;
}

static int write_one_block(int fd, void *buf, int *err)
{
    int ret = dev_write(fd, buf, MOFS_BLK_SIZE);

    if (ret == -1) {
        *err = get_errno();
        ret  = 0;
    } else {
        *err = 0;
    }

    return ret;
}

int write_continuous_blocks(int fd, void *buf, unsigned int blk_num, unsigned int *written_blk_num, size_t *fraction)
{
    int err = 0;
    int ret = 0;

    if ((fd < 0) || (buf == NULL) || (written_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* Align check */
    if ((dev_lseek(fd, 0, MOFS_SEEK_CUR) % MOFS_BLK_SIZE) != 0) {
        ret = MOFS_EINVAL;
    }

    *fraction        = 0U;
    *written_blk_num = 0U;

    if (ret == 0) {
        for (int i = 0; i < blk_num; i++) {
            ret = write_one_block(fd, buf, &err);
            if (ret == 0) {
                ret = err;
                break;
            } else if (ret != MOFS_BLK_SIZE) {
                *fraction = ret;
                ret       = 0;
                break;
            }
            *written_blk_num = *written_blk_num + 1;
            buf              = (char *)buf + MOFS_BLK_SIZE;
        }
    }

    return ret;
}
