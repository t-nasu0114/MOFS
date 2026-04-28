#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>

/* mofs_format is currently defined in mofs_format.c without public header. */
extern int mofs_format(const char *device_file, int fs_size, int blk_size);

static void test_TC_P1_009_format_with_explicit_size(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P1_010_format_with_device_size(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P1_011_format_with_invalid_path(void **state)
{
    int ret = 0;

    (void)state;
    ret = mofs_format("/tmp/path_that_should_not_exist_mofs", 1, MOFS_BLK_SIZE);
    assert_true(ret != 0);
}

static void test_TC_P1_012_format_then_init(void **state)
{
    (void)state;
    skip();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P1_009_format_with_explicit_size),
        cmocka_unit_test(test_TC_P1_010_format_with_device_size),
        cmocka_unit_test(test_TC_P1_011_format_with_invalid_path),
        cmocka_unit_test(test_TC_P1_012_format_then_init),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
