#ifndef __MOFS_BUFFER__
#define __MOFS_BUFFER__

#include <mofs_types.h>

/* internal block buffer cache (write-back) */
int  mofs_bcache_init(void);
void mofs_bcache_fini(void);
int  mofs_bcache_read_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                             unsigned int *read_blk_num, mofs_size_t *fraction);
int  mofs_bcache_write_blocks(int fd, const void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                              unsigned int *written_blk_num, mofs_size_t *fraction);
int  mofs_bcache_flush(void);
int  mofs_bcache_invalidate(unsigned int blk_num);

#endif /* __MOFS_BUFFER__ */
