
#ifndef __MOFS_CORE__
#define __MOFS_CORE__

#ifndef bool
#define bool  unsigned int
#define true  1U
#define false 0U
#endif /* bool */

#include <mofs_struct.h>

typedef struct mofs_ctx
{
    bool              init;
    char             *dev_path;
    int               dev_fd;
    mofs_superblock_t sp_blk;
} mofs_ctx_t;

extern mofs_ctx_t ctx;

int mofs_init_core(const char *path);
int mofs_fini_core(void);

#endif /* __MOFS_CORE__ */
