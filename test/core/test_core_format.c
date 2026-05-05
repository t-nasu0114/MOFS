#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_errno.h>

#include "../fixtures/test_fixture.h"

/* mofs_format is currently defined in mofs_format.c without public header. */
extern int mofs_format(const char *device_file, int fs_size, int blk_size);

/* TC-P1-009: format succeeds when explicit filesystem size is provided. */
static void test_TC_P1_009_format_with_explicit_size(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_format(image_path, 256, MOFS_BLK_SIZE);
    assert_int_equal(ret, 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

/* TC-P1-010: format succeeds when size is taken from device file. */
static void test_TC_P1_010_format_with_device_size(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_format(image_path, 0, MOFS_BLK_SIZE);
    assert_int_equal(ret, 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

/* TC-P1-011: format fails for invalid target path. */
static void test_TC_P1_011_format_with_invalid_path(void **state)
{
    int ret = 0;

    (void)state;
    ret = mofs_format("/tmp/path_that_should_not_exist_mofs", 1, MOFS_BLK_SIZE);
    assert_true(ret != 0);
}

/* TC-P1-012: init_core succeeds after successful format. */
static void test_TC_P1_012_format_then_init(void **state)
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

static void test_format_blk_size_default_via_negative_one(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_format(image_path, 0, -1);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, 0);
    assert_int_equal(mofs_fini_core(), 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

static void test_format_blk_size_512(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_format(image_path, 0, 512);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, 0);
    assert_int_equal(mofs_fini_core(), 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

static void test_format_blk_size_4096(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_format(image_path, 0, 4096);
    assert_int_equal(ret, 0);

    ret = mofs_init_core(image_path);
    assert_int_equal(ret, 0);
    assert_int_equal(mofs_fini_core(), 0);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

static void test_format_blk_size_1536_rejected(void **state)
{
    char image_path[128] = {0};
    int  ret             = 0;

    (void)state;
    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    assert_int_equal(ret, 0);

    ret = mofs_format(image_path, 0, 1536);
    assert_int_equal(ret, MOFS_EINVAL);
    assert_int_equal(mofs_test_remove_file(image_path), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P1_009_format_with_explicit_size),
        cmocka_unit_test(test_TC_P1_010_format_with_device_size),
        cmocka_unit_test(test_TC_P1_011_format_with_invalid_path),
        cmocka_unit_test(test_TC_P1_012_format_then_init),
        cmocka_unit_test(test_format_blk_size_default_via_negative_one),
        cmocka_unit_test(test_format_blk_size_512),
        cmocka_unit_test(test_format_blk_size_4096),
        cmocka_unit_test(test_format_blk_size_1536_rejected),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
