#ifndef __MOFS_POSIX__
#define __MOFS_POSIX__

#include <mofs_core.h>
#include <mofs_dir.h>
#include <mofs_file.h>

/* Functions Declarations */
int               mofs_fstat(int fd, mofs_stat_t *stbuf);
mofs_dirhandle_t *mofs_opendir(const char *path);
int               mofs_closedir(mofs_dirhandle_t *handle);
mofs_dirent_t    *mofs_readdir(mofs_dirhandle_t *handle);
mofs_filehandle_t *mofs_open(const char *path, int flags, mode_t mode);
int                mofs_close(mofs_filehandle_t *handle);
int                mofs_read(mofs_filehandle_t *handle, void *buf, size_t size);

#endif /* __MOFS_POSIX__ */