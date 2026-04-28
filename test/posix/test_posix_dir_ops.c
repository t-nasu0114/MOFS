#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <errno.h>
#include <mofs_posix.h>

static void test_TC_P0_012_opendir_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_013_opendir_not_directory(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_014_opendir_missing_path(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_015_readdir_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_016_readdir_eof(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_017_closedir_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P0_018_closedir_invalid_handle(void **state)
{
    int ret = 0;

    (void)state;
    errno = 0;
    ret   = mofs_closedir(NULL);
    assert_int_equal(ret, -1);
    assert_true(errno != 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P0_012_opendir_success),
        cmocka_unit_test(test_TC_P0_013_opendir_not_directory),
        cmocka_unit_test(test_TC_P0_014_opendir_missing_path),
        cmocka_unit_test(test_TC_P0_015_readdir_success),
        cmocka_unit_test(test_TC_P0_016_readdir_eof),
        cmocka_unit_test(test_TC_P0_017_closedir_success),
        cmocka_unit_test(test_TC_P0_018_closedir_invalid_handle),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
