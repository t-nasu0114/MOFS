#include <mofs_errno.h>
#include <mofs_port_errno.h>

/* Zephyr stub: identity / no-op errno bridge for link-time completeness. */

int get_errno(void)
{
    return MOFS_ENOSYS;
}

int os_to_mofs_errno(int os_errno)
{
    if (os_errno == 0) {
        return 0;
    }
    return MOFS_ENOSYS;
}

int mofs_to_os_errno(int mofs_errno)
{
    if (mofs_errno == 0) {
        return 0;
    }
    return mofs_errno;
}
