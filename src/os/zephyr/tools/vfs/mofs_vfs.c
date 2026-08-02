#include "mofs_vfs.h"

#include <mofs_lifecycle.h>
#include <mofs_port_errno.h>
#include <mofs_types.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/fs_sys.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mofs_vfs, CONFIG_MOFS_LOG_LEVEL);

static int mofs_mount(struct fs_mount_t *mount);
static int mofs_unmount(struct fs_mount_t *mount);

static struct fs_file_system_t mofs_fs = {
    .mount   = mofs_mount,
    .unmount = mofs_unmount,
};

/**
 * @brief Mount MOFS through Zephyr VFS.
 *
 * Function behavior:
 * - Treats `mount->storage_dev` as a disk_access disk name string.
 * - Calls `mofs_init_core()` with fixed root uid/gid 0.
 *
 * @param[in] mount Zephyr mount descriptor.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_mount(struct fs_mount_t *mount)
{
    int         err;
    const char *disk;

    if ((mount == NULL) || (mount->storage_dev == NULL)) {
        return -EINVAL;
    }

    disk = (const char *)mount->storage_dev;
    err  = mofs_init_core(disk, MOFS_TRUE, 0U, 0U);
    if (err != 0) {
        return -mofs_to_os_errno(err);
    }
    return 0;
}

/**
 * @brief Unmount MOFS through Zephyr VFS.
 *
 * Function behavior:
 * - Calls `mofs_fini_core()` to release the single global MOFS context.
 *
 * @param[in] mount Zephyr mount descriptor (unused).
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_unmount(struct fs_mount_t *mount)
{
    int err;

    (void)mount;
    err = mofs_fini_core();
    if (err != 0) {
        return -mofs_to_os_errno(err);
    }
    return 0;
}

/**
 * @brief Register MOFS with Zephyr VFS at POST_KERNEL time.
 *
 * @return 0 on success.
 * @return Negative errno from `fs_register()` on failure.
 */
static int mofs_init_zvfs(void)
{
    int err = fs_register(MOFS_FS_TYPE, &mofs_fs);

    if (err != 0) {
        LOG_ERR("Failed to register MOFS filesystem: %d", err);
        return err;
    }
    LOG_INF("MOFS filesystem registered (type=%d)", MOFS_FS_TYPE);
    return 0;
}

SYS_INIT(mofs_init_zvfs, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
