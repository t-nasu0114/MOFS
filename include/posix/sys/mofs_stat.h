#ifndef __MOFS_POSIX_SYS_STAT__
#define __MOFS_POSIX_SYS_STAT__

#include <mofs_type.h>

/* File type (POSIX sys/stat.h equivalent) */
#define MOFS_FTYPE_DIR 0040000U /* Directory.  */
#define MOFS_FTYPE_REG 0100000U /* Regular file.  */

typedef struct mofs_stat
{
    ino_t    st_ino;
    mode_t   st_mode;
    nlink_t  st_nlink;
    uid_t    st_uid;
    gid_t    st_gid;
    off_t    st_size;
    int64_t  st_atime_sec;
    int64_t  st_mtime_sec;
    int64_t  st_ctime_sec;
    uint32_t st_blocks;
} mofs_stat_t;

int mofs_stat(const char *path, mofs_stat_t *stbuf);

#endif /* __MOFS_POSIX_SYS_STAT__ */
