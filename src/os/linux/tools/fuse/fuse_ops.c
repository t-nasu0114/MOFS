/* fuse_ops.c
 * FUSE operation callbacks for MOFS.
 */

#include "fuse_ops.h"
#include <fuse.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fuse_operations op = {
    .init    = mofs_init,
    .destroy = mofs_destroy,
    .getattr = mofs_getattr,
    .readdir = mofs_readdir,
    .read    = mofs_read,
};

/**
 * @brief Initialize MOFS core during FUSE startup.
 *
 * Function behavior:
 * - Obtains FUSE private context from `fuse_get_context()`.
 * - Initializes MOFS core using the configured device path.
 * - Terminates the process if initialization fails.
 *
 * @param[in] conn FUSE connection information (unused).
 * @param[in,out] cfg FUSE configuration (currently unused).
 * @param[out] none No output parameters.
 * @return Pointer to FUSE private data on success.
 */
void *mofs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    int ret = 0;
    (void)conn;
    struct fuse_context *context  = fuse_get_context();
    mofs_fuse_ctx_t     *fuse_ctx = (mofs_fuse_ctx_t *)context->private_data;

    ret = mofs_init_core(fuse_ctx->devfile_path);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize MOFS core: %d\n", ret);
        exit(EXIT_FAILURE);
    }
    return (void *)(context->private_data);
}

/**
 * @brief Finalize MOFS core during FUSE shutdown.
 *
 * Function behavior:
 * - Calls `mofs_fini_core()` to release MOFS resources.
 *
 * @param[in] private_data FUSE private data pointer (unused).
 * @param[out] none No output parameters.
 * @return No return value.
 */
void mofs_destroy(void *private_data)
{
    (void)private_data;
    mofs_fini_core();
}

/**
 * @brief Retrieve file attributes for a path.
 *
 * Function behavior:
 * - Supports root path lookup (`/` and `/.`).
 * - Resolves inode number and reads inode metadata from MOFS.
 * - Fills `struct stat` and converts MOFS errors to OS errno values.
 *
 * @param[in] path Target path string.
 * @param[out] stbuf Output `stat` structure to populate.
 * @param[in] fi FUSE file info (unused).
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi;
    int          ret       = 0;
    int          inode_num = 0;
    mofs_inode_t inode;
    memset(stbuf, 0, sizeof(struct stat));

    ret = mofs_getattr_core(path, &inode_num, &inode);
    if (ret == 0) {
        stbuf->st_ino   = inode_num;
        stbuf->st_nlink = inode.i_links;
        stbuf->st_size  = inode.i_size;
        stbuf->st_mode  = inode.i_mode;
        stbuf->st_uid   = inode.i_uid;
        stbuf->st_gid   = inode.i_gid;
    }
    return -(mofs_to_os_errno(ret));
}

/**
 * @brief Read directory entries for a path.
 *
 * Function behavior:
 * - Supports root directory listing (`/` and `/.`).
 * - Adds `.` and `..` entries via FUSE filler callback.
 *
 * @param[in] path Target directory path.
 * @param[out] buf FUSE directory buffer passed to filler callback.
 * @param[in] filler Callback used to append directory entries.
 * @param[in] offset Directory read offset (unused).
 * @param[in] fi FUSE file info (unused).
 * @param[in] flags FUSE readdir flags (unused).
 * @return 0 on success.
 * @return -ENOENT if the path is not supported.
 */
int mofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags)
{
    (void)filler;
    (void)offset;
    (void)fi;
    (void)flags;

    /* return root dir */
    if ((strcmp(path, "/") == 0) || (strcmp(path, "/.") == 0)) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        return 0;
    }
    return -(mofs_to_os_errno(MOFS_ENOENT));
}

/**
 * @brief Read file data for a path.
 *
 * Function behavior:
 * - Placeholder implementation. File read operation is not implemented yet.
 *
 * @param[in] path Target file path.
 * @param[out] buf Destination buffer for read data.
 * @param[in] size Number of bytes requested.
 * @param[in] offset Read offset in bytes.
 * @param[in] fi FUSE file info.
 * @return Negative OS errno for `MOFS_ENOSYS` because this operation is not
 *         implemented.
 */
int mofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;

    return -(mofs_to_os_errno(MOFS_ENOSYS));
}
