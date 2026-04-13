#ifndef __MOFS_POSIX__
#define __MOFS_POSIX__

#include <mofs_core.h>
#include <mofs_dir.h>

/* Functions Declarations */
int               mofs_fstat(int fd, mofs_stat_t *stbuf);
mofs_dirhandle_t *mofs_opendir(const char *path);
int               mofs_closedir(mofs_dirhandle_t *handle);
mofs_dirent_t    *mofs_readdir(mofs_dirhandle_t *handle);

#endif /* __MOFS_POSIX__ */