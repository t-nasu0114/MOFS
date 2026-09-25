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
#define MOFS_TEST_DIR     "/MOFS/dir"
#define MOFS_TEST_CHILD   "/MOFS/dir/a.txt"
#define MOFS_TEST_RENAMED "/MOFS/dir/b.txt"

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
 * @brief Create a directory, list it, rename a child, sync, and unlink.
 *
 * @return 0 when each VFS call matches the expected directory state.
 * @return Negative errno from the failing VFS call.
 */
static int mofs_vfs_exercise_dir(void)
{
    struct fs_file_t file;
    struct fs_dir_t  dir;
    struct fs_dirent entry;
    int              found = 0;
    int              ret;

    ret = fs_mkdir(MOFS_TEST_DIR);
    if (ret < 0) {
        LOG_ERR("fs_mkdir(\"%s\") failed: %d", MOFS_TEST_DIR, ret);
        return ret;
    }

    fs_file_t_init(&file);
    ret = fs_open(&file, MOFS_TEST_CHILD, FS_O_CREATE | FS_O_RDWR);
    if (ret < 0) {
        LOG_ERR("fs_open(\"%s\") failed: %d", MOFS_TEST_CHILD, ret);
        return ret;
    }
    ret = fs_sync(&file);
    if (ret < 0) {
        LOG_ERR("fs_sync failed: %d", ret);
        (void)fs_close(&file);
        return ret;
    }
    ret = fs_close(&file);
    if (ret < 0) {
        LOG_ERR("fs_close failed: %d", ret);
        return ret;
    }

    ret = fs_stat(MOFS_TEST_CHILD, &entry);
    if ((ret < 0) || (entry.type != FS_DIR_ENTRY_FILE)) {
        LOG_ERR("fs_stat(\"%s\") failed: %d type=%d", MOFS_TEST_CHILD, ret, (int)entry.type);
        return (ret < 0) ? ret : -EIO;
    }

    fs_dir_t_init(&dir);
    ret = fs_opendir(&dir, MOFS_TEST_DIR);
    if (ret < 0) {
        LOG_ERR("fs_opendir(\"%s\") failed: %d", MOFS_TEST_DIR, ret);
        return ret;
    }
    while (true) {
        ret = fs_readdir(&dir, &entry);
        if (ret < 0) {
            LOG_ERR("fs_readdir failed: %d", ret);
            (void)fs_closedir(&dir);
            return ret;
        }
        if (entry.name[0] == '\0') {
            break;
        }
        if (strcmp(entry.name, "a.txt") == 0) {
            found = 1;
        }
    }
    ret = fs_closedir(&dir);
    if (ret < 0) {
        LOG_ERR("fs_closedir failed: %d", ret);
        return ret;
    }
    if (found == 0) {
        LOG_ERR("fs_readdir missed \"a.txt\"");
        return -EIO;
    }

    ret = fs_rename(MOFS_TEST_CHILD, MOFS_TEST_RENAMED);
    if (ret < 0) {
        LOG_ERR("fs_rename failed: %d", ret);
        return ret;
    }

    ret = fs_unlink(MOFS_TEST_RENAMED);
    if (ret < 0) {
        LOG_ERR("fs_unlink(\"%s\") failed: %d", MOFS_TEST_RENAMED, ret);
        return ret;
    }
    ret = fs_unlink(MOFS_TEST_DIR);
    if (ret < 0) {
        LOG_ERR("fs_unlink(\"%s\") failed: %d", MOFS_TEST_DIR, ret);
        return ret;
    }

    LOG_INF("dir ops OK: \"%s\"", MOFS_TEST_DIR);
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

    ret = mofs_vfs_exercise_dir();
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
