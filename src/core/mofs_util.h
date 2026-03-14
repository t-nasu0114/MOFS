#ifndef __MOFS_UTIL__
#define __MOFS_UTIL__

#include <stddef.h>

int read_continuous_blocks(int fd, void *buf, unsigned int blk_num, unsigned int *read_blk_num, size_t *fraction);

#endif /* __MOFS_UTIL__ */