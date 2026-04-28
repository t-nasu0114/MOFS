#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_errno.h>

static void test_TC_P1_001_init_core_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P1_002_init_core_missing_device(void **state)
{
    int ret = 0;

    (void)state;
    ret = mofs_init_core("/tmp/path_that_should_not_exist_mofs");
    assert_true(ret != 0);
}

static void test_TC_P1_003_init_core_magic_mismatch(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P1_004_fini_core_returns_zero(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P1_005_stat_core_success(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P1_006_stat_core_null_path(void **state)
{
    int         ret   = 0;
    mofs_stat_t stbuf = {0};

    (void)state;
    ret = mofs_stat_core(NULL, &stbuf);
    assert_int_equal(ret, MOFS_EINVAL);
}

static void test_TC_P1_007_stat_core_null_stbuf(void **state)
{
    int ret = 0;

    (void)state;
    ret = mofs_stat_core("/", NULL);
    assert_int_equal(ret, MOFS_EINVAL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P1_001_init_core_success),
        cmocka_unit_test(test_TC_P1_002_init_core_missing_device),
        cmocka_unit_test(test_TC_P1_003_init_core_magic_mismatch),
        cmocka_unit_test(test_TC_P1_004_fini_core_returns_zero),
        cmocka_unit_test(test_TC_P1_005_stat_core_success),
        cmocka_unit_test(test_TC_P1_006_stat_core_null_path),
        cmocka_unit_test(test_TC_P1_007_stat_core_null_stbuf),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
