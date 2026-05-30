#ifndef __MOFS_INODE__
#define __MOFS_INODE__

#include <mofs_core.h>

#define MOFS_INODE_TIME_ATIME 0x1U
#define MOFS_INODE_TIME_MTIME 0x2U
#define MOFS_INODE_TIME_CTIME 0x4U
#define MOFS_INODE_TIME_ALL   (MOFS_INODE_TIME_ATIME | MOFS_INODE_TIME_MTIME | MOFS_INODE_TIME_CTIME)

int mofs_read_inode(int inode_num, mofs_inode_t *inode);
int mofs_write_inode(int inode_num, const mofs_inode_t *inode);
int allocate_inode(int *inode_num);
int free_inode(int inode_num);
int mofs_path_to_inode_num(const char *path, int *inode_num);
int mofs_inode_stamp_now(mofs_inode_t *inode, unsigned int mask);

#endif /* __MOFS_INODE__ */
