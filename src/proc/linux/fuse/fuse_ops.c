/* fuse_ops.c
 * FUSE operation callbacks for MOFS.
 */

#include "fuse_ops.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>

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
