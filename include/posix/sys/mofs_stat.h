#ifndef __MOFS_POSIX_SYS_STAT__
#define __MOFS_POSIX_SYS_STAT__

#include <mofs_types.h>

/* File type (POSIX sys/stat.h equivalent) */
#define MOFS_FTYPE_DIR 0040000U /* Directory.  */
#define MOFS_FTYPE_REG 0100000U /* Regular file.  */

typedef struct mofs_stat
{
    mofs_ino_t    st_ino;
    mofs_mode_t   st_mode;
    mofs_nlink_t  st_nlink;
    mofs_uid_t    st_uid;
    mofs_gid_t    st_gid;
    mofs_off_t    st_size;
    mofs_int64_t  st_atime_sec;
    mofs_int64_t  st_mtime_sec;
    mofs_int64_t  st_ctime_sec;
    mofs_uint32_t st_blocks;
} mofs_stat_t;

int mofs_stat(const char *path, mofs_stat_t *stbuf);

#endif /* __MOFS_POSIX_SYS_STAT__ */
