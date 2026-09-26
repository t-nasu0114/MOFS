#ifndef __MOFS_VFS_H__
#define __MOFS_VFS_H__

#include <zephyr/fs/fs.h>

/** External FS type slot used with fs_register / fs_mount. */
#define MOFS_FS_TYPE_INDEX 1
#define MOFS_FS_TYPE       (FS_TYPE_EXTERNAL_BASE + MOFS_FS_TYPE_INDEX)

#endif /* __MOFS_VFS_H__ */