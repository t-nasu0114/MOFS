#ifndef __MOFS_POSIX_UNISTD__
#define __MOFS_POSIX_UNISTD__

#include <mofs_type.h>

typedef struct mofs_filehandle mofs_filehandle_t;

int mofs_close(mofs_filehandle_t *handle);
int mofs_read(mofs_filehandle_t *handle, void *buf, size_t size);
int mofs_write(mofs_filehandle_t *handle, const void *buf, size_t size);
int mofs_pread(mofs_filehandle_t *handle, void *buf, size_t size, off_t offset);
int mofs_pwrite(mofs_filehandle_t *handle, const void *buf, size_t size, off_t offset);
int mofs_truncate(const char *path, off_t length);
int mofs_ftruncate(mofs_filehandle_t *handle, off_t length);
int mofs_unlink(const char *path);
int mofs_mkdir(const char *path, mode_t mode);
int mofs_rmdir(const char *path);

#endif /* __MOFS_POSIX_UNISTD__ */
