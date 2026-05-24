
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_posix.h>
#include <mofs_path.h>
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
int mofs_init_core(const char *path, bool update_root_owner, uint32_t root_uid, uint32_t root_gid)
{
    int                ret = 0;
    int                nr  = 0;
    mofs_superblock_t  sb_scratch;
    unsigned long long vol_bytes = 0;
    mofs_inode_t       root_inode;

    mofs_memset(&sb_scratch, 0, sizeof(sb_scratch));

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

    /* Read superblock from block 0 (structure fits in first logical block). */
    if (dev_lseek(ctx.dev_fd, 0, MOFS_SEEK_SET) < 0) {
        ret = get_errno();
        goto out3;
    }

    nr = dev_read(ctx.dev_fd, &sb_scratch, sizeof(sb_scratch));
    if (nr != (int)sizeof(sb_scratch)) {
        ret = MOFS_EIO;
        MOFS_ERR("Superblock read failed");
        goto out3;
    }

    if (sb_scratch.magic != MOFS_MAGIC_NUM) {
        ret = MOFS_EIO;
        MOFS_ERR("Device is not formatted");
        goto out3;
    }

    ret = mofs_validate_logical_blk_size(sb_scratch.blk_size);
    if (ret != 0) {
        MOFS_ERR("Invalid superblock block size");
        goto out3;
    }

    vol_bytes = dev_get_size(ctx.dev_fd, &ret);
    if (ret != 0) {
        MOFS_ERR("Get device size error");
        goto out3;
    }
    /* Device tail bytes smaller than one logical block are ignored. */
    if ((vol_bytes / (unsigned long long)sb_scratch.blk_size) != (unsigned long long)sb_scratch.hole_blk_num) {
        ret = MOFS_EINVAL;
        MOFS_ERR("Superblock volume size mismatch");
        goto out3;
    }

    ctx.sp_blk = sb_scratch;

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

    if (update_root_owner == true) {
        ret = mofs_read_inode(MOFS_ROOT_INODE_NUM, &root_inode);
        if (ret != 0) {
            goto out3;
        }
        root_inode.i_uid = root_uid;
        root_inode.i_gid = root_gid;
        ret              = mofs_write_inode(MOFS_ROOT_INODE_NUM, &root_inode);
        if (ret != 0) {
            goto out3;
        }
    }

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