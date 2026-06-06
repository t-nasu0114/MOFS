#ifndef __MOFS_PORT_ERRNO__
#define __MOFS_PORT_ERRNO__

int os_to_mofs_errno(int os_errno);
int mofs_to_os_errno(int mofs_errno);
int get_errno(void);

#endif /* __MOFS_PORT_ERRNO__ */
