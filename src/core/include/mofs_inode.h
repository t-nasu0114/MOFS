#ifndef __MOFS_INODE__
#define __MOFS_INODE__

#include <mofs_core.h>

int mofs_read_inode(int inode_num, mofs_inode_t *inode);
int mofs_write_inode(int inode_num, const mofs_inode_t *inode);
int mofs_path_to_inode_num(const char *path, int *inode_num);

#endif /* __MOFS_INODE__ */
