#ifndef __MOFS_PERM__
#define __MOFS_PERM__

#include <mofs_core.h>
#include <mofs_inode.h>
#include <mofs_port_user.h>
#include <posix/mofs_fcntl.h>

int mofs_check_open_permission(int flags, const mofs_user_ctx_t *user, const mofs_inode_t *inode);
int mofs_check_dir_traverse(const mofs_user_ctx_t *user, const mofs_inode_t *dir_inode);
int mofs_check_dir_read(const mofs_user_ctx_t *user, const mofs_inode_t *dir_inode);
int mofs_check_dir_write(const mofs_user_ctx_t *user, const mofs_inode_t *dir_inode);

#endif /* __MOFS_PERM__ */
