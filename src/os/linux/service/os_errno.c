#include <errno.h>
#include <mofs_errno.h>

int get_errno(void)
{
    return os_to_mofs_errno(errno);
}

int os_to_mofs_errno(int os_errno)
{
    if (os_errno == 0) {
        return 0;
    }

    switch (os_errno) {
    case EPERM:
        return MOFS_EPERM;
    case ENOENT:
        return MOFS_ENOENT;
    case EIO:
        return MOFS_EIO;
    case ENXIO:
        return MOFS_ENXIO;
    case EBADF:
        return MOFS_EBADF;
    case ENOMEM:
        return MOFS_ENOMEM;
    case EACCES:
        return MOFS_EACCES;
    case EFAULT:
        return MOFS_EFAULT;
    case EBUSY:
        return MOFS_EBUSY;
    case EEXIST:
        return MOFS_EEXIST;
    case EXDEV:
        return MOFS_EXDEV;
    case ENODEV:
        return MOFS_ENODEV;
    case ENOTDIR:
        return MOFS_ENOTDIR;
    case EISDIR:
        return MOFS_EISDIR;
    case EINVAL:
        return MOFS_EINVAL;
    case ENFILE:
        return MOFS_ENFILE;
    case EMFILE:
        return MOFS_EMFILE;
    case EFBIG:
        return MOFS_EFBIG;
    case ENOSPC:
        return MOFS_ENOSPC;
    case ESPIPE:
        return MOFS_ESPIPE;
    case EROFS:
        return MOFS_EROFS;
    case EMLINK:
        return MOFS_EMLINK;
    case ENAMETOOLONG:
        return MOFS_ENAMETOOLONG;
    case ENOSYS:
        return MOFS_ENOSYS;
    case ENOTEMPTY:
        return MOFS_ENOTEMPTY;
    case ELOOP:
        return MOFS_ELOOP;
#if defined(EOPNOTSUPP)
    case EOPNOTSUPP:
        return MOFS_ENOTSUP;
#endif
#if defined(ENOTSUP) && (!defined(EOPNOTSUPP) || (ENOTSUP != EOPNOTSUPP))
    case ENOTSUP:
        return MOFS_ENOTSUP;
#endif
    default:
        return MOFS_EIO;
    }
}

int mofs_to_os_errno(int mofs_errno)
{
    if (mofs_errno == 0) {
        return 0;
    }

    switch (mofs_errno) {
    case MOFS_EPERM:
        return EPERM;
    case MOFS_ENOENT:
        return ENOENT;
    case MOFS_EIO:
        return EIO;
    case MOFS_ENXIO:
        return ENXIO;
    case MOFS_EBADF:
        return EBADF;
    case MOFS_ENOMEM:
        return ENOMEM;
    case MOFS_EACCES:
        return EACCES;
    case MOFS_EFAULT:
        return EFAULT;
    case MOFS_EBUSY:
        return EBUSY;
    case MOFS_EEXIST:
        return EEXIST;
    case MOFS_EXDEV:
        return EXDEV;
    case MOFS_ENODEV:
        return ENODEV;
    case MOFS_ENOTDIR:
        return ENOTDIR;
    case MOFS_EISDIR:
        return EISDIR;
    case MOFS_EINVAL:
        return EINVAL;
    case MOFS_ENFILE:
        return ENFILE;
    case MOFS_EMFILE:
        return EMFILE;
    case MOFS_EFBIG:
        return EFBIG;
    case MOFS_ENOSPC:
        return ENOSPC;
    case MOFS_ESPIPE:
        return ESPIPE;
    case MOFS_EROFS:
        return EROFS;
    case MOFS_EMLINK:
        return EMLINK;
    case MOFS_ENAMETOOLONG:
        return ENAMETOOLONG;
    case MOFS_ENOSYS:
        return ENOSYS;
    case MOFS_ENOTEMPTY:
        return ENOTEMPTY;
    case MOFS_ELOOP:
        return ELOOP;
    case MOFS_ENOTSUP:
#if defined(ENOTSUP)
        return ENOTSUP;
#elif defined(EOPNOTSUPP)
        return EOPNOTSUPP;
#else
        return EIO;
#endif
    default:
        return EIO;
    }
}