#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_posix.h>

static void test_TC_P1_008_fstat_stub_returns_zero(void **state)
{
    int         ret   = 0;
    mofs_stat_t stbuf = {0};

    (void)state;
    ret = mofs_fstat(0, &stbuf);
    assert_int_equal(ret, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P1_008_fstat_stub_returns_zero),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
