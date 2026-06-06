#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_posix.h>
#include <mofs_user.h>
#include <unistd.h>

#include "../fixtures/test_fixture.h"

extern int mofs_format(const char *device_file, int fs_size, int blk_size);

static int setup_timestamp_fixture(void **state)
{
    static char image_path[128];
    int         ret = 0;

    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 4U * 1024U * 1024U);
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
    ret = mofs_init_core(image_path, false, 0U, 0U);
    if (ret != 0) {
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }

    *state = image_path;
    return 0;
}

static int teardown_timestamp_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

static void test_inode_size_is_64_bytes(void **state)
{
    (void)state;
    assert_int_equal((int)sizeof(mofs_inode_t), 64);
}

static void test_root_stat_has_timestamps(void **state)
{
    mofs_stat_t st;

    (void)state;
    assert_int_equal(mofs_stat("/", &st), 0);
    assert_true(st.st_atime_sec > 0);
    assert_true(st.st_mtime_sec > 0);
    assert_true(st.st_ctime_sec > 0);
}

static void test_create_sets_file_timestamps(void **state)
{
    mofs_stat_t        st;
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/newfile.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/newfile.txt", &st), 0);
    assert_true(st.st_atime_sec > 0);
    assert_true(st.st_mtime_sec > 0);
    assert_true(st.st_ctime_sec > 0);
}

static void test_write_updates_mtime_and_ctime(void **state)
{
    mofs_stat_t        st_before;
    mofs_stat_t        st_after;
    mofs_filehandle_t *handle = NULL;
    const char         data[] = "data";

    (void)state;
    handle = mofs_open("/write_ts.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/write_ts.txt", &st_before), 0);
    (void)sleep(1U);
    handle = mofs_open("/write_ts.txt", MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, (void *)data, sizeof(data)), (ssize_t)sizeof(data));
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/write_ts.txt", &st_after), 0);
    assert_true(st_after.st_mtime_sec >= st_before.st_mtime_sec);
    assert_true(st_after.st_ctime_sec >= st_before.st_ctime_sec);
    assert_true(st_after.st_mtime_sec > st_before.st_mtime_sec);
}

static void test_read_updates_atime(void **state)
{
    mofs_stat_t        st_before;
    mofs_stat_t        st_after;
    mofs_filehandle_t *handle = NULL;
    char               buf[8] = {0};

    (void)state;
    handle = mofs_open("/read_ts.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, (void *)"x", 1U), 1);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/read_ts.txt", &st_before), 0);
    (void)sleep(1U);
    handle = mofs_open("/read_ts.txt", MOFS_OFLAG_RDONLY, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_read(handle, buf, sizeof(buf)), 1);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/read_ts.txt", &st_after), 0);
    assert_true(st_after.st_atime_sec >= st_before.st_atime_sec);
    assert_true(st_after.st_atime_sec > st_before.st_atime_sec);
}

static void test_mkdir_updates_parent_mtime(void **state)
{
    mofs_stat_t root_before;
    mofs_stat_t root_after;

    (void)state;
    assert_int_equal(mofs_stat("/", &root_before), 0);
    (void)sleep(1U);
    assert_int_equal(mofs_mkdir("/subdir", 0755U), 0);

    assert_int_equal(mofs_stat("/", &root_after), 0);
    assert_true(root_after.st_mtime_sec >= root_before.st_mtime_sec);
    assert_true(root_after.st_ctime_sec >= root_before.st_ctime_sec);
    assert_true(root_after.st_mtime_sec > root_before.st_mtime_sec);
}

static void test_unlink_updates_parent_mtime(void **state)
{
    mofs_stat_t        root_before;
    mofs_stat_t        root_after;
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/unlink_me.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/", &root_before), 0);
    (void)sleep(1U);
    assert_int_equal(mofs_unlink("/unlink_me.txt"), 0);

    assert_int_equal(mofs_stat("/", &root_after), 0);
    assert_true(root_after.st_mtime_sec >= root_before.st_mtime_sec);
    assert_true(root_after.st_ctime_sec >= root_before.st_ctime_sec);
    assert_true(root_after.st_mtime_sec > root_before.st_mtime_sec);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_inode_size_is_64_bytes),
        cmocka_unit_test_setup_teardown(test_root_stat_has_timestamps, setup_timestamp_fixture,
                                        teardown_timestamp_fixture),
        cmocka_unit_test_setup_teardown(test_create_sets_file_timestamps, setup_timestamp_fixture,
                                        teardown_timestamp_fixture),
        cmocka_unit_test_setup_teardown(test_write_updates_mtime_and_ctime, setup_timestamp_fixture,
                                        teardown_timestamp_fixture),
        cmocka_unit_test_setup_teardown(test_read_updates_atime, setup_timestamp_fixture,
                                        teardown_timestamp_fixture),
        cmocka_unit_test_setup_teardown(test_mkdir_updates_parent_mtime, setup_timestamp_fixture,
                                        teardown_timestamp_fixture),
        cmocka_unit_test_setup_teardown(test_unlink_updates_parent_mtime, setup_timestamp_fixture,
                                        teardown_timestamp_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
