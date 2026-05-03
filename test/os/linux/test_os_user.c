#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_errno.h>
#include <mofs_type.h>
#include <mofs_user.h>

/* TC-P2-009: set/get caller user preserves uid/gid/pid values. */
static void test_TC_P2_009_set_and_get_caller_user(void **state)
{
    mofs_user_ctx_t user;
    int             ret = 0;

    (void)state;
    ret = mofs_set_caller_user((uid_t)1001, (gid_t)1002, (pid_t)1003);
    assert_int_equal(ret, 0);

    ret = mofs_get_caller_user(&user);
    assert_int_equal(ret, 0);
    assert_int_equal((int)user.uid, 1001);
    assert_int_equal((int)user.gid, 1002);
    assert_int_equal((int)user.pid, 1003);
    assert_true(user.valid);
}

/* TC-P2-010: setting supplementary groups with NULL pointer fails with EINVAL. */
static void test_TC_P2_010_set_supp_groups_with_null_group_ptr(void **state)
{
    int ret = 0;

    (void)state;
    ret = mofs_set_caller_supp_groups(NULL, 1U);
    assert_int_equal(ret, MOFS_EINVAL);
}

/* TC-P2-011: caller is recognized as member of its primary group. */
static void test_TC_P2_011_is_caller_in_group_primary_group(void **state)
{
    bool is_member = false;
    int  ret       = 0;

    (void)state;
    ret = mofs_set_caller_user((uid_t)2001, (gid_t)2002, (pid_t)2003);
    assert_int_equal(ret, 0);

    ret = mofs_is_caller_in_group((gid_t)2002, &is_member);
    assert_int_equal(ret, 0);
    assert_true(is_member);
}

/* TC-P2-012: non-member group query returns false with success status. */
static void test_TC_P2_012_is_caller_in_group_not_member(void **state)
{
    bool is_member = true;
    int  ret       = 0;

    (void)state;
    ret = mofs_set_caller_user((uid_t)3001, (gid_t)3002, (pid_t)3003);
    assert_int_equal(ret, 0);

    ret = mofs_is_caller_in_group((gid_t)3999, &is_member);
    assert_int_equal(ret, 0);
    assert_false(is_member);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_TC_P2_009_set_and_get_caller_user),
        cmocka_unit_test(test_TC_P2_010_set_supp_groups_with_null_group_ptr),
        cmocka_unit_test(test_TC_P2_011_is_caller_in_group_primary_group),
        cmocka_unit_test(test_TC_P2_012_is_caller_in_group_not_member),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
