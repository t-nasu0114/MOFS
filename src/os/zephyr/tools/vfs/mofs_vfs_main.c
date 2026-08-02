/**
 * @file mofs_vfs_main.c
 * @brief Zephyr host app to exercise MOFS VFS registration and mount.
 *
 * Build (from a Zephyr workspace):
 *
 *   west build -p always -b qemu_cortex_r5 \
 *     /path/to/MOFS/src/os/zephyr/tools/vfs -- \
 *     -DEXTRA_ZEPHYR_MODULES=/path/to/MOFS
 *
 *   west build -t run
 */

#include "mofs_vfs.h"

#include <mofs_format.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mofs_vfs_main, LOG_LEVEL_INF);

#define MOFS_DISK_NAME   "RAM"
#define MOFS_MNT_POINT   "/MOFS"
#define MOFS_BLK_SIZE_APP 4096

static struct fs_mount_t mofs_mnt = {
    .type        = MOFS_FS_TYPE,
    .mnt_point   = MOFS_MNT_POINT,
    .fs_data     = NULL,
    .storage_dev = (void *)MOFS_DISK_NAME,
};

/**
 * @brief Format the RAM disk, mount MOFS via VFS, then unmount.
 *
 * Function behavior:
 * - Formats disk `RAM` with `mofs_format()` (blk size 4096, auto fs size).
 * - Mounts at `/MOFS` using the registered MOFS file-system type.
 * - Unmounts and reports each step over the log backend.
 *
 * @return 0 on success.
 * @return Non-zero when format, mount, or unmount fails.
 */
int main(void)
{
    int ret;

    LOG_INF("MOFS VFS host starting");

    ret = mofs_format(MOFS_DISK_NAME, -1, MOFS_BLK_SIZE_APP);
    if (ret != 0) {
        LOG_ERR("mofs_format(\"%s\") failed: %d", MOFS_DISK_NAME, ret);
        return 1;
    }
    LOG_INF("formatted disk \"%s\" (blk_size=%d)", MOFS_DISK_NAME, MOFS_BLK_SIZE_APP);

    ret = fs_mount(&mofs_mnt);
    if (ret < 0) {
        LOG_ERR("fs_mount(\"%s\") failed: %d", MOFS_MNT_POINT, ret);
        return 1;
    }
    LOG_INF("mounted MOFS at \"%s\"", MOFS_MNT_POINT);

    ret = fs_unmount(&mofs_mnt);
    if (ret < 0) {
        LOG_ERR("fs_unmount(\"%s\") failed: %d", MOFS_MNT_POINT, ret);
        return 1;
    }
    LOG_INF("unmounted \"%s\"", MOFS_MNT_POINT);

    LOG_INF("MOFS VFS host finished OK");
    return 0;
}
