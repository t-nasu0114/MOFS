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

#include <errno.h>
#include <mofs_format.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mofs_vfs_main, LOG_LEVEL_INF);

#define MOFS_DISK_NAME    "RAM"
#define MOFS_MNT_POINT    "/MOFS"
#define MOFS_BLK_SIZE_APP 4096
#define MOFS_TEST_PATH    "/MOFS/hello.txt"
#define MOFS_TEST_PAYLOAD "hello-mofs"

static struct fs_mount_t mofs_mnt = {
    .type        = MOFS_FS_TYPE,
    .mnt_point   = MOFS_MNT_POINT,
    .fs_data     = NULL,
    .storage_dev = (void *)MOFS_DISK_NAME,
};

/**
 * @brief Create a file, write a payload, seek back, and read it.
 *
 * @return 0 when the bytes read match the payload.
 * @return Negative errno from the failing VFS call.
 */
static int mofs_vfs_exercise_file(void)
{
    struct fs_file_t file;
    char             buf[sizeof(MOFS_TEST_PAYLOAD)] = {0};
    ssize_t          n;
    int              ret;

    fs_file_t_init(&file);

    ret = fs_open(&file, MOFS_TEST_PATH, FS_O_CREATE | FS_O_RDWR);
    if (ret < 0) {
        LOG_ERR("fs_open(\"%s\") failed: %d", MOFS_TEST_PATH, ret);
        return ret;
    }

    n = fs_write(&file, MOFS_TEST_PAYLOAD, strlen(MOFS_TEST_PAYLOAD));
    if (n < 0) {
        LOG_ERR("fs_write failed: %d", (int)n);
        (void)fs_close(&file);
        return (int)n;
    }
    if ((size_t)n != strlen(MOFS_TEST_PAYLOAD)) {
        LOG_ERR("fs_write short: %d", (int)n);
        (void)fs_close(&file);
        return -EIO;
    }

    ret = fs_seek(&file, 0, FS_SEEK_SET);
    if (ret < 0) {
        LOG_ERR("fs_seek failed: %d", ret);
        (void)fs_close(&file);
        return ret;
    }

    n = fs_read(&file, buf, strlen(MOFS_TEST_PAYLOAD));
    if (n < 0) {
        LOG_ERR("fs_read failed: %d", (int)n);
        (void)fs_close(&file);
        return (int)n;
    }
    if (((size_t)n != strlen(MOFS_TEST_PAYLOAD)) || (memcmp(buf, MOFS_TEST_PAYLOAD, (size_t)n) != 0)) {
        LOG_ERR("fs_read mismatch (%d bytes)", (int)n);
        (void)fs_close(&file);
        return -EIO;
    }

    ret = fs_close(&file);
    if (ret < 0) {
        LOG_ERR("fs_close failed: %d", ret);
        return ret;
    }

    LOG_INF("file I/O OK: \"%s\" (%s)", MOFS_TEST_PATH, MOFS_TEST_PAYLOAD);
    return 0;
}

/**
 * @brief Format the RAM disk, mount MOFS, exercise file I/O, then unmount.
 *
 * Function behavior:
 * - Formats disk `RAM` with `mofs_format()` (blk size 4096, auto fs size).
 * - Mounts at `/MOFS` using the registered MOFS file-system type.
 * - Creates `/MOFS/hello.txt`, writes a payload, seeks to the start, and reads it back.
 * - Unmounts and reports each step over the log backend.
 *
 * @return 0 on success.
 * @return Non-zero when format, mount, file I/O, or unmount fails.
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

    ret = mofs_vfs_exercise_file();
    if (ret < 0) {
        (void)fs_unmount(&mofs_mnt);
        return 1;
    }

    ret = fs_unmount(&mofs_mnt);
    if (ret < 0) {
        LOG_ERR("fs_unmount(\"%s\") failed: %d", MOFS_MNT_POINT, ret);
        return 1;
    }
    LOG_INF("unmounted \"%s\"", MOFS_MNT_POINT);

    LOG_INF("MOFS VFS host finished OK");
    return 0;
}
