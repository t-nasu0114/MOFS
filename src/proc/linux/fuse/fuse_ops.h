/* fuse_ops.h
 * Declarations of FUSE operation callbacks for MOFS.
 * Implemented in fuse_ops.c.
 */

#ifndef MOFS_FUSE_OPS_H
#define MOFS_FUSE_OPS_H

#include <fuse.h>

int mofs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int mofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags);
int mofs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi);

#endif /* MOFS_FUSE_OPS_H */
