#ifndef __MOFS_UTIL__
#define __MOFS_UTIL__

#include <stddef.h>

int read_continuous_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                           unsigned int *read_blk_num, size_t *fraction);
int read_file_data_block(int inode_num, void *buf, unsigned int start_blk_num, size_t *fraction);
int write_continuous_blocks(int fd, void *buf, unsigned int blk_num, unsigned int *written_blk_num, size_t *fraction);
int write_file_data_block(int inode_num, void *buf, unsigned int start_blk_num, size_t *fraction);
int find_dir_entry(char *component, int parent_inode_num, int *child_inode_num);

#endif /* __MOFS_UTIL__ */