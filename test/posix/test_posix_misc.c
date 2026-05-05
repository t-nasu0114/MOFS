#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <errno.h>
#include <mofs_core.h>
#include <mofs_file.h>
#include <mofs_posix.h>
#include <mofs_user.h>
#include <unistd.h>

#include "../fixtures/test_fixture.h"

/* mofs_format is currently defined in mofs_format.c without public header. */
extern int mofs_format(const char *device_file, int fs_size, int blk_size);

static int setup_posix_misc_fixture(void **state)
{
    static char image_path[128];
    int         ret = 0;

    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 2U * 1024U * 1024U);
    if (ret != 0) {
        return -1;
    }
    ret = mofs_format(image_path, 0, MOFS_BLK_SIZE);
    if (ret != 0) {
        (void)mofs_test_remove_file(image_path);
        return -1;
    }
    ret = mofs_set_caller_user((uid_t)0, (gid_t)0, getpid());
    if (ret != 0) {
        (void)mofs_test_remove_file(image_path);
        return -1;
    }
    ret = mofs_init_core(image_path);
    if (ret != 0) {
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }

    *state = image_path;
    return 0;
}

static int teardown_posix_misc_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

/* TC-P1-008: stat root path succeeds and reports a directory. */
static void test_TC_P1_008_stat_root(void **state)
{
    mofs_stat_t stbuf = {0};
    int         ret   = 0;

    (void)state;
    errno = 0;
    ret   = mofs_stat("/", &stbuf);
    assert_int_equal(ret, 0);
    assert_int_equal(errno, 0);
    assert_int_not_equal((unsigned int)stbuf.st_mode & MOFS_FTYPE_DIR, 0U);
}

/* TC-P1-009: stat missing path fails with ENOENT. */
static void test_TC_P1_009_stat_missing_path(void **state)
{
    mofs_stat_t stbuf = {0};
    int         ret   = 0;

    (void)state;
    errno = 0;
    ret   = mofs_stat("/no_such_file", &stbuf);
    assert_int_equal(ret, -1);
    assert_int_equal(errno, ENOENT);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_TC_P1_008_stat_root, setup_posix_misc_fixture, teardown_posix_misc_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P1_009_stat_missing_path, setup_posix_misc_fixture, teardown_posix_misc_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
