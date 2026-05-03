#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <errno.h>
#include <mofs_errno.h>

/* TC-P2-005: os_to_mofs_errno maps EINVAL to MOFS_EINVAL. */
static void test_TC_P2_005_os_to_mofs_errno_einval(void **state)
{
    (void)state;
    assert_int_equal(os_to_mofs_errno(EINVAL), MOFS_EINVAL);
}

/* TC-P2-006: os_to_mofs_errno falls back to MOFS_EIO for unknown errno. */
static void test_TC_P2_006_os_to_mofs_errno_unknown(void **state)
{
    (void)state;
    assert_int_equal(os_to_mofs_errno(123456), MOFS_EIO);
}

/* TC-P2-007: mofs_to_os_errno maps MOFS_ENOENT to ENOENT. */
static void test_TC_P2_007_mofs_to_os_errno_enoent(void **state)
{
    (void)state;
    assert_int_equal(mofs_to_os_errno(MOFS_ENOENT), ENOENT);
}

/* TC-P2-008: mofs_to_os_errno falls back to EIO for unknown MOFS errno. */
static void test_TC_P2_008_mofs_to_os_errno_unknown(void **state)
{
    (void)state;
    assert_int_equal(mofs_to_os_errno(999999), EIO);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P2_005_os_to_mofs_errno_einval),
        cmocka_unit_test(test_TC_P2_006_os_to_mofs_errno_unknown),
        cmocka_unit_test(test_TC_P2_007_mofs_to_os_errno_enoent),
        cmocka_unit_test(test_TC_P2_008_mofs_to_os_errno_unknown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
