#ifndef __MOFS_ERRNO__
#define __MOFS_ERRNO__

#define MOFS_EPERM        1  /* Operation not permitted */
#define MOFS_ENOENT       2  /* No such file or directory */
#define MOFS_EIO          5  /* I/O error */
#define MOFS_ENXIO        6  /* No such device or address */
#define MOFS_EBADF        9  /* Bad file number */
#define MOFS_ENOMEM       12 /* Out of memory */
#define MOFS_EACCES       13 /* Permission denied */
#define MOFS_EFAULT       14 /* Bad address */
#define MOFS_EBUSY        16 /* Device or resource busy */
#define MOFS_EEXIST       17 /* File exists */
#define MOFS_EXDEV        18 /* Cross-device link */
#define MOFS_ENODEV       19 /* No such device */
#define MOFS_ENOTDIR      20 /* Not a directory */
#define MOFS_EISDIR       21 /* Is a directory */
#define MOFS_EINVAL       22 /* Invalid argument */
#define MOFS_ENFILE       23 /* File table overflow */
#define MOFS_EMFILE       24 /* Too many open files */
#define MOFS_EFBIG        27 /* File too large */
#define MOFS_ENOSPC       28 /* No space left on device */
#define MOFS_ESPIPE       29 /* Illegal seek */
#define MOFS_EROFS        30 /* Read-only file system */
#define MOFS_EMLINK       31 /* Too many links */
#define MOFS_ENAMETOOLONG 36 /* File name too long */
#define MOFS_ENOSYS       38 /* Function not implemented */
#define MOFS_ENOTEMPTY    39 /* Directory not empty */
#define MOFS_ELOOP        40 /* Too many symbolic links encountered */

int os_to_mofs_errno(int os_errno);
int mofs_to_os_errno(int mofs_errno);
int get_errno(void);

#endif /* __MOFS_ERRNO__ */
