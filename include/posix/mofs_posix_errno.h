#ifndef __MOFS_POSIX_ERRNO__
#define __MOFS_POSIX_ERRNO__

int *mofs_errno_location(void);
#define mofs_errno (*mofs_errno_location())

#endif /* __MOFS_POSIX_ERRNO__ */
