#ifndef __MOFS_BLOCK__
#define __MOFS_BLOCK__

#include <mofs_type.h>

int read_continuous_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                           unsigned int *read_blk_num, size_t *fraction);
int write_continuous_blocks(int fd, const void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                            unsigned int *written_blk_num, size_t *fraction);

#endif /* __MOFS_BLOCK__ */
