/* fuse_ops.c
 * FUSE operation callbacks for MOFS.
 */

#include "fuse_ops.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fuse.h>
#include <mofs_core.h>
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

void mofs_destroy(void *private_data)
{
    (void)private_data;
    mofs_fini_core();
}

int mofs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    /* only root */
    if ((strcmp(path, "/") == 0) || (strcmp(path, "/.") == 0)) {
        stbuf->st_ino   = 2;
        stbuf->st_nlink = 2;
        stbuf->st_size  = 4096;
        stbuf->st_mode  = 0040000U | 0755;
        return 0;
    }
    return -ENOENT;
}

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
    return -ENOENT;
}

int mofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;

    return -ENOSYS;
}
