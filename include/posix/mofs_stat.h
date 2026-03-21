#ifndef __MOFS_STAT__
#define __MOFS_STAT__

#include <mofs_type.h>

typedef struct mofs_stat
{
    ino_t   st_ino;
    mode_t  st_mode;
    nlink_t st_nlink;
    uid_t   st_uid;
    gid_t   st_gid;
    off_t   st_size;
    /* Time info is not supported yet */
    uint32_t st_blocks;
} mofs_stat_t;

/* Functions Declarations */
int mofs_fstat(int fd, mofs_stat_t *stbuf);

#endif /* __MOFS_STAT__ */