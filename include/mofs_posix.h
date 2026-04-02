#ifndef __MOFS_POSIX__
#define __MOFS_POSIX__

#include "posix/mofs_stat.h"
#include <mofs_dir.h>

/* Functions Declarations */
mofs_dirhandle_t *mofs_opendir(const char *path);
int               mofs_closedir(mofs_dirhandle_t *handle);
mofs_dirent_t    *mofs_readdir(mofs_dirhandle_t *handle);

#endif /* __MOFS_POSIX__ */