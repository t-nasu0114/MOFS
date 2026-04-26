
#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_posix.h>
#include <mofs_str.h>
#include <mofs_type.h>
#include <mofs_util.h>

mofs_ctx_t ctx = {.init = false, .dev_path = NULL, .dev_fd = 0};

/**
 * @brief Initialize the MOFS core context and load the device superblock.
 *
 * Function behavior:
 * - Stores the device path and opens the target device.
 * - Reads the superblock from block 0 and validates the magic number.
 * - Copies the validated superblock fields into the global context `ctx`
 *   and marks initialization complete (`ctx.init = true`).
 *
 * @param[in] path NULL-terminated device path string to open.
 * @param[out] none No output parameters. The global context `ctx` is updated.
 * @return 0 on success.
 * @return Non-zero on failure.
 *         - Value returned by get_errno(): system-related errors such as
 *           memory allocation failure, device open failure, or block read
 *           failure.
 *         - MOFS_EIO: superblock magic mismatch (for example, unformatted
 *           device).
 */
int mofs_init_core(const char *path)
{
    int          ret          = 0;
    unsigned int read_blk_num = 0;
    size_t       fraction     = 0;
    void        *buf          = NULL;

    /* Open device */
    ctx.dev_path = mofs_malloc(mofs_strlen(path) + 1);
    if (ctx.dev_path == NULL) {
        ret = get_errno();
        goto out1;
    }

    mofs_strcpy(ctx.dev_path, path);

    ctx.dev_fd = dev_open(path, MOFS_IO_OPEN_FLAG_RDWR | MOFS_IO_OPEN_FLAG_SYNC);
    if (ctx.dev_fd < 0) {
        ret = get_errno();
        goto out2;
    }

    /* Read superblock */
    buf = mofs_malloc(MOFS_BLK_SIZE);
    if (buf == NULL) {
        ret = get_errno();
        goto out3;
    }

    ret = read_continuous_blocks(ctx.dev_fd, buf, 1, 0, &read_blk_num, &fraction);
    if ((ret != 0) || (read_blk_num != 1)) {
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

    /* Initialize directory handle pool */
    mofs_memset(dirhandle_pool, 0, sizeof(dirhandle_pool));
    for (int i = 0; i < MOFS_DIRHANDLE_POOL_SIZE; i++) {
        dirhandle_pool[i].used = false;
    }

    /* Initialize file handle pool */
    mofs_memset(filehandle_pool, 0, sizeof(filehandle_pool));
    for (int i = 0; i < MOFS_FILEHANDLE_POOL_SIZE; i++) {
        filehandle_pool[i].used = false;
    }

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

/**
 * @brief Finalize the MOFS core context and release allocated resources.
 *
 * Function behavior:
 * - Closes the currently opened device.
 * - Frees the stored device path string.
 * - Resets the global context `ctx` to an uninitialized state.
 *
 * @param[in] none No input parameters.
 * @param[out] none No output parameters. The global context `ctx` is updated.
 * @return 0. This function always returns 0.
 */
int mofs_fini_core(void)
{
    dev_close(ctx.dev_fd);
    mofs_free(ctx.dev_path);
    ctx.dev_path = NULL;
    ctx.dev_fd   = 0;
    ctx.init     = false;
    return 0;
}

/**
 * @brief Resolve a path and read its inode metadata.
 *
 * Function behavior:
 * - Validates input pointers.
 * - Resolves inode number from the specified path.
 * - Reads inode contents for the resolved inode number.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[out] stbuf Destination pointer for resolved inode number.
 * @return 0 on success.
 * @return MOFS_EINVAL if any argument is invalid.
 * @return Non-zero error returned by `mofs_path_to_inode_num()` or `mofs_read_inode()`
 *         when lookup/read fails.
 */
int mofs_stat_core(const char *path, mofs_stat_t *stbuf)
{
    int          ret       = 0;
    int          inode_num = -1;
    mofs_inode_t inode;

    if ((path == NULL) || (stbuf == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* search inode number from path */
    if (ret == 0) {
        mofs_memset(stbuf, 0, sizeof(mofs_stat_t));
        mofs_memset(&inode, 0, sizeof(mofs_inode_t));

        ret = mofs_path_to_inode_num(path, &inode_num);
        if (ret == 0) {
            ret = mofs_read_inode(inode_num, &inode);
            if (ret == 0) {
                stbuf->st_ino   = inode_num;
                stbuf->st_nlink = inode.i_links;
                stbuf->st_size  = inode.i_size;
                stbuf->st_mode  = inode.i_mode;
                stbuf->st_uid   = inode.i_uid;
                stbuf->st_gid   = inode.i_gid;
            }
        }
    }

    return ret;
}