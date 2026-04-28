#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <errno.h>
#include <mofs_posix.h>

static void test_TC_P0_001_open_existing_rdonly(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_002_open_missing_path(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_003_open_without_accmode(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_004_open_with_creat_on_missing_path(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_005_open_directory_flag_for_regular_file(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_006_read_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_007_read_with_null_handle(void **state)
{
    int ret = 0;

    (void)state;
    errno = 0;
    ret   = mofs_read(NULL, NULL, 1U);
    assert_int_equal(ret, -1);
    assert_int_equal(errno, EINVAL);
}

static void test_TC_P0_008_read_with_zero_size(void **state)
{
    int ret = 0;

    (void)state;
    errno = 0;
    ret   = mofs_read(NULL, NULL, 0U);
    assert_int_equal(ret, -1);
    assert_int_equal(errno, EINVAL);
}

static void test_TC_P0_009_read_at_eof(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_010_close_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_011_close_with_invalid_handle(void **state)
{
    int ret = 0;

    (void)state;
    errno = 0;
    ret   = mofs_close(NULL);
    assert_true(ret != 0);
    assert_true(errno != 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P0_001_open_existing_rdonly),
        cmocka_unit_test(test_TC_P0_002_open_missing_path),
        cmocka_unit_test(test_TC_P0_003_open_without_accmode),
        cmocka_unit_test(test_TC_P0_004_open_with_creat_on_missing_path),
        cmocka_unit_test(test_TC_P0_005_open_directory_flag_for_regular_file),
        cmocka_unit_test(test_TC_P0_006_read_success),
        cmocka_unit_test(test_TC_P0_007_read_with_null_handle),
        cmocka_unit_test(test_TC_P0_008_read_with_zero_size),
        cmocka_unit_test(test_TC_P0_009_read_at_eof),
        cmocka_unit_test(test_TC_P0_010_close_success),
        cmocka_unit_test(test_TC_P0_011_close_with_invalid_handle),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
