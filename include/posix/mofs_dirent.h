#ifndef __MOFS_POSIX_DIRENT__
#define __MOFS_POSIX_DIRENT__

#include <mofs_type.h>
#include <posix/mofs_limits.h>

typedef struct mofs_dirent
{
    char     name[MOFS_FILENAME_LEN];
    uint32_t inode_num;
} mofs_dirent_t;

typedef struct mofs_dirhandle mofs_dirhandle_t;

mofs_dirhandle_t *mofs_opendir(const char *path);
int               mofs_closedir(mofs_dirhandle_t *handle);
mofs_dirent_t    *mofs_readdir(mofs_dirhandle_t *handle);

#endif /* __MOFS_POSIX_DIRENT__ */
