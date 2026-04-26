/* fuse_ops.h
 * Declarations of FUSE operation callbacks for MOFS.
 * Implemented in fuse_ops.c.
 */

#ifndef MOFS_FUSE_OPS_H
#define MOFS_FUSE_OPS_H

#include <fuse.h>

void *mofs_init_fuse(struct fuse_conn_info *conn, struct fuse_config *cfg);
void  mofs_destroy_fuse(void *private_data);
int   mofs_getattr_fuse(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int   mofs_open_fuse(const char *path, struct fuse_file_info *fi);
int   mofs_release_fuse(const char *path, struct fuse_file_info *fi);
int   mofs_readdir_fuse(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags);
int   mofs_read_fuse(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

typedef struct
{
    char *devfile_path; /* Device file path used by MOFS backend (opened on DEVICE_FILE). */
} mofs_fuse_ctx_t;

extern struct fuse_operations op;

#endif /* MOFS_FUSE_OPS_H */
