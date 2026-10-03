#ifndef __MOFS_POSIX_FCNTL__
#define __MOFS_POSIX_FCNTL__

#include <mofs_types.h>

/* Supported open flags (POSIX fcntl.h equivalent) */
#define MOFS_OFLAG_RDONLY    0x0001
#define MOFS_OFLAG_WRONLY    0x0002
#define MOFS_OFLAG_RDWR      (MOFS_OFLAG_RDONLY | MOFS_OFLAG_WRONLY)
#define MOFS_OFLAG_EXEC      0x0004
#define MOFS_OFLAG_SEARCH    0x0008
#define MOFS_OFLAG_ACCMODE   (MOFS_OFLAG_RDWR | MOFS_OFLAG_EXEC | MOFS_OFLAG_SEARCH)
#define MOFS_OFLAG_CREAT     0x0010
#define MOFS_OFLAG_DIRECTORY 0x0020
#define MOFS_OFLAG_EXCL      0x0040
#define MOFS_OFLAG_TRUNC     0x0080 /* Writable open truncates a regular file to 0. Read-only is EINVAL. */
#define MOFS_OFLAG_APPEND    0x0100 /* mofs_write appends at EOF. mofs_pwrite keeps the given offset. */
#define MOFS_OFLAG_SYNC      0x0200 /* write, pwrite, open(O_TRUNC), and ftruncate flush the volume cache. */

typedef struct mofs_filehandle mofs_filehandle_t;

mofs_filehandle_t *mofs_open(const char *path, int flags, mofs_mode_t mode);

#endif /* __MOFS_POSIX_FCNTL__ */
