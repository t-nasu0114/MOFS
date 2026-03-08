
#include "mofs_log.h"
#include "mofs_util.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_mem.h>
#include <mofs_struct.h>
#include <stddef.h>
#include <string.h>

mofs_ctx_t ctx = {.init = false, .dev_path = NULL, .dev_fd = 0};

int mofs_init_core(const char *path)
{
    int ret = 0;

    /* Open device */
    ctx.dev_path = mofs_malloc(strlen(path) + 1);
    if (ctx.dev_path == NULL) {
        ret = get_errno();
        goto out1;
    }

    strcpy(ctx.dev_path, path);

    ctx.dev_fd = dev_open(path, MOFS_IO_OPEN_FLAG_RDWR | MOFS_IO_OPEN_FLAG_SYNC);
    if (ctx.dev_fd < 0) {
        ret = get_errno();
        goto out2;
    }

    /* Read superblock */
    void *buf = mofs_malloc(MOFS_BLK_SIZE);
    if (buf == NULL) {
        ret = get_errno();
        goto out3;
    }
    ret = read_continuous_blocks(ctx.dev_fd, buf, 1);
    if (ret != 0) {
        ret = get_errno();
        mofs_free(buf);
        goto out3;
    }

    if (((mofs_superblock_t *)buf)->magic != MOFS_MAGIC_NUM) {
        ret = MOFS_EIO;
        MOFS_ERR("Device is not formatted");
        goto out3;
    }

    ctx.sp_blk.magic              = ((mofs_superblock_t *)buf)->magic;
    ctx.sp_blk.hole_blk_num       = ((mofs_superblock_t *)buf)->hole_blk_num;
    ctx.sp_blk.inode_num          = ((mofs_superblock_t *)buf)->inode_num;
    ctx.sp_blk.data_blk_num       = ((mofs_superblock_t *)buf)->data_blk_num;
    ctx.sp_blk.inode_bitmap_start = ((mofs_superblock_t *)buf)->inode_bitmap_start;
    ctx.sp_blk.data_bitmap_start  = ((mofs_superblock_t *)buf)->data_bitmap_start;
    ctx.sp_blk.inode_table_start  = ((mofs_superblock_t *)buf)->inode_table_start;
    ctx.sp_blk.data_region_start  = ((mofs_superblock_t *)buf)->data_region_start;

    mofs_free(buf);

    /* Mark as initalized */
    ctx.init = true;
    return 0;

out3:
    dev_close(ctx.dev_fd);
out2:
    mofs_free(ctx.dev_path);
out1:
    ctx.dev_path = NULL;
    ctx.dev_fd   = 0;
    ctx.init     = false;
    return ret;
}

int mofs_fini_core(void)
{
    dev_close(ctx.dev_fd);
    mofs_free(ctx.dev_path);
    ctx.dev_path = NULL;
    ctx.dev_fd   = 0;
    ctx.init     = false;
    return 0;
}
