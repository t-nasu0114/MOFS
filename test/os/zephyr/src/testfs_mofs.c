/**
 * @file testfs_mofs.c
 * @brief Mount fixture for Zephyr's common fs_* tests.
 */

#include "mofs_vfs.h"

#include <mofs_format.h>
#include <zephyr/fs/fs.h>
#include <zephyr/ztest.h>

#define MOFS_TEST_DISK "RAM"
#define MOFS_TEST_MNT  "/MOFS"
#define MOFS_TEST_BLK  4096

static struct fs_mount_t mofs_mnt = {
    .type        = MOFS_FS_TYPE,
    .mnt_point   = MOFS_TEST_MNT,
    .storage_dev = (void *)MOFS_TEST_DISK,
};

struct fs_mount_t *fs_basic_test_mp  = &mofs_mnt;
struct fs_mount_t *fs_dirops_test_mp = &mofs_mnt;

static char mofs_open_flags_path[] = MOFS_TEST_MNT "/the_file";
char       *test_fs_open_flags_file_path = mofs_open_flags_path;

void test_fs_basic(void);
void test_fs_dirops(void);
void test_fs_open_flags(void);

/**
 * @brief Drop a previous mount and format the RAM disk.
 *
 * Function behavior:
 * - Ignores `-EINVAL` from `fs_unmount()` when nothing is mounted.
 * - Formats disk `RAM` with block size 4096.
 */
static void mofs_test_reformat(void)
{
    int ret;

    ret = fs_unmount(&mofs_mnt);
    if ((ret != 0) && (ret != -EINVAL)) {
        zassert_equal(ret, 0, "unmount before format failed: %d", ret);
    }
    zassert_equal(mofs_format(MOFS_TEST_DISK, -1, MOFS_TEST_BLK), 0, "mofs_format failed");
}

/**
 * @brief Run the common file I/O tests on a freshly formatted volume.
 */
ZTEST(mofs, test_basic)
{
    mofs_test_reformat();
    test_fs_basic();
}

/**
 * @brief Run the common directory tests on a freshly formatted volume.
 */
ZTEST(mofs, test_dirops)
{
    mofs_test_reformat();
    test_fs_dirops();
}

/**
 * @brief Run the common open-flag tests on a freshly formatted volume.
 */
ZTEST(mofs, test_open_flags)
{
    int ret;

    mofs_test_reformat();
    mofs_mnt.flags = 0;
    ret            = fs_mount(&mofs_mnt);
    zassert_equal(ret, 0, "fs_mount failed: %d", ret);

    test_fs_open_flags();

    ret = fs_unmount(&mofs_mnt);
    zassert_equal(ret, 0, "fs_unmount failed: %d", ret);
}
