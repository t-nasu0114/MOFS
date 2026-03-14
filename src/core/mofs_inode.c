#include "mofs_util.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_mem.h>
#include <stddef.h>
#include <stdint.h>

int mofs_read_inode(int fd, int inode_num, mofs_inode_t *inode)
{
    int          ret          = 0;
    off_t        blk_offset   = 0;
    off_t        inode_offset = 0;
    unsigned int read_blk_num = 0;
    size_t       fraction     = 0;
    void        *buf          = NULL;
    void        *inode_ptr    = NULL;

    if (fd < 0 || inode_num < 0 || inode == NULL) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        buf = mofs_malloc(MOFS_BLK_SIZE);
        if (buf == NULL) {
            ret = get_errno();
        }
    }

    blk_offset = (ctx.sp_blk.inode_table_start * MOFS_BLK_SIZE);
    blk_offset += (inode_num * sizeof(mofs_inode_t)) / MOFS_BLK_SIZE;
    inode_offset = (inode_num * sizeof(mofs_inode_t)) % MOFS_BLK_SIZE;

    if (ret == 0) {
        if (dev_lseek(fd, blk_offset, MOFS_SEEK_SET) < 0) {
            ret = get_errno();
        }
    }

    if (ret == 0) {
        ret = read_continuous_blocks(fd, buf, 1, &read_blk_num, &fraction);
        if ((ret != 0) || (read_blk_num != 1) || (fraction != 0)) {
            ret = get_errno();
        } else {
            inode_ptr = (char *)buf + inode_offset;
            mofs_memcpy(inode, inode_ptr, sizeof(mofs_inode_t));
        }
    }

    if (buf != NULL) {
        mofs_free(buf);
    }

    return ret;
}

int mofs_path_to_inode_num(const char *path, int *inode_num)
{
    int ret    = 0;
    *inode_num = 0;

    if ((path == NULL) || (path[0] != '/')) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        if ((path[0] == '/') && (path[1] == '\0')) {
            *inode_num = 2;
        } else {
            ret = MOFS_EINVAL;
        }
    }

    return ret;
}