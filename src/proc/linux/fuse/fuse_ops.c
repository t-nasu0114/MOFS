/* fuse_ops.c
 * FUSE operation callbacks for MOFS.
 */

#include "fuse_ops.h"
#include <errno.h>

int mofs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)path;
    (void)stbuf;
    (void)fi;
    return -ENOSYS;
}

int mofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi,
               enum fuse_readdir_flags flags)
{
    (void)path;
    (void)buf;
    (void)filler;
    (void)offset;
    (void)fi;
    (void)flags;
    return -ENOSYS;
}

int mofs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -ENOSYS;
}
