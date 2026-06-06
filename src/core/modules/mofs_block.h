#ifndef __MOFS_BLOCK__
#define __MOFS_BLOCK__

#include <mofs_types.h>

int allocate_data_block(int inode_num, unsigned int req_blk_num);
int free_data_block(int inode_num, unsigned int start_blk_num, unsigned int req_blk_num);
int resolve_file_data_block(int inode_num, unsigned int file_blk_idx, unsigned int *abs_blk_out);
int read_continuous_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                           unsigned int *read_blk_num, mofs_size_t *fraction);
int write_continuous_blocks(int fd, const void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                            unsigned int *written_blk_num, mofs_size_t *fraction);

#endif /* __MOFS_BLOCK__ */
