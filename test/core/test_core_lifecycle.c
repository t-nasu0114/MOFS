#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <errno.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_posix.h>

#include "../fixtures/test_fixture.h"

/* mofs_format is currently defined in mofs_format.c without public header. */
extern int mofs_format(const char *device_file, int fs_size, int blk_size);

/* TC-P1-001: init_core succeeds on formatted image. */
static void test_TC_P1_001_init_core_success(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);
    ret = mofs_format(image_path, 0, MOFS_BLK_SIZE);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, 0);

    assert_int_equal(mofs_fini_core(), 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

/* TC-P1-002: init_core fails on missing device path. */
static void test_TC_P1_002_init_core_missing_device(void **state)
{
    int ret = 0;

    (void)state;
    ret = mofs_init_core("/tmp/path_that_should_not_exist_mofs");
    assert_true(ret != 0);
}

/* TC-P1-003: init_core reports MOFS_EIO for magic mismatch image. */
static void test_TC_P1_003_init_core_magic_mismatch(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, MOFS_EIO);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

/* TC-P1-004: fini_core returns success after successful init. */
static void test_TC_P1_004_fini_core_returns_zero(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);
    ret = mofs_format(image_path, 0, MOFS_BLK_SIZE);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, 0);

    ret = mofs_fini_core();
    assert_int_equal(ret, 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

/* TC-P1-005: mofs_stat returns valid root inode information. */
static void test_TC_P1_005_stat_success(void **state)
{
    char        image_path[128] = {0};
    int         ret             = 0;
    mofs_stat_t stbuf           = {0};

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);
    ret = mofs_format(image_path, 0, MOFS_BLK_SIZE);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, 0);

    errno = 0;
    ret   = mofs_stat("/", &stbuf);
    assert_int_equal(ret, 0);
    assert_int_equal(errno, 0);
    assert_int_equal((int)stbuf.st_ino, 2);
    assert_true((stbuf.st_mode & MOFS_FTYPE_DIR) != 0U);

    assert_int_equal(mofs_fini_core(), 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

/* TC-P1-006: mofs_stat rejects NULL path. */
static void test_TC_P1_006_stat_null_path(void **state)
{
    int         ret   = 0;
    mofs_stat_t stbuf = {0};

    (void)state;
    errno = 0;
    ret   = mofs_stat(NULL, &stbuf);
    assert_int_equal(ret, -1);
    assert_int_equal(errno, EINVAL);
}

/* TC-P1-007: mofs_stat rejects NULL output buffer. */
static void test_TC_P1_007_stat_null_stbuf(void **state)
{
    int ret = 0;

    (void)state;
    errno = 0;
    ret   = mofs_stat("/", NULL);
    assert_int_equal(ret, -1);
    assert_int_equal(errno, EINVAL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P1_001_init_core_success),
        cmocka_unit_test(test_TC_P1_002_init_core_missing_device),
        cmocka_unit_test(test_TC_P1_003_init_core_magic_mismatch),
        cmocka_unit_test(test_TC_P1_004_fini_core_returns_zero),
        cmocka_unit_test(test_TC_P1_005_stat_success),
        cmocka_unit_test(test_TC_P1_006_stat_null_path),
        cmocka_unit_test(test_TC_P1_007_stat_null_stbuf),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
