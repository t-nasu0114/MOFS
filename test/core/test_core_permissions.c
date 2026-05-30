#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <errno.h>
#include <mofs_core.h>
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_posix.h>
#include <mofs_user.h>
#include <posix/sys/mofs_stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../fixtures/test_fixture.h"

extern int mofs_format(const char *device_file, int fs_size, int blk_size);

static int setup_permissions_fixture(void **state)
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

static int teardown_permissions_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

static int set_path_mode_bits(const char *path, uint16_t perm_bits)
{
    int          ret       = 0;
    int          inode_num = -1;
    mofs_inode_t inode;

    ret = mofs_path_to_inode_num(path, &inode_num);
    if (ret != 0) {
        return ret;
    }
    ret = mofs_read_inode(inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    inode.i_mode = (uint16_t)((inode.i_mode & (MOFS_FTYPE_DIR | MOFS_FTYPE_REG)) | (perm_bits & 07777U));
    return mofs_write_inode(inode_num, &inode);
}

static void test_traverse_denied_without_execute_on_parent(void **state)
{
    int          ret       = 0;
    int          inode_num = -1;
    mofs_stat_t  st;

    (void)state;

    assert_int_equal(mofs_mkdir("/locked", (mode_t)(MOFS_FTYPE_DIR | 0700U)), 0);
    assert_int_equal(mofs_mkdir("/locked/open", (mode_t)(MOFS_FTYPE_DIR | 0777U)), 0);

    assert_int_equal(mofs_set_caller_user((uid_t)1001, (gid_t)1001, getpid()), 0);

    ret = mofs_create_core("/locked/open/file.txt", 0644U, &inode_num);
    assert_int_equal(ret, MOFS_EACCES);

    ret = mofs_mkdir_core("/locked/open/sub", (mode_t)(MOFS_FTYPE_DIR | 0777U));
    assert_int_equal(ret, MOFS_EACCES);

    assert_int_equal(mofs_stat("/locked/open", &st), -1);
    assert_int_equal(errno, EACCES);
}

static void test_create_denied_without_search_on_parent(void **state)
{
    int ret       = 0;
    int inode_num = -1;

    (void)state;

    assert_int_equal(mofs_set_caller_user((uid_t)0, (gid_t)0, getpid()), 0);
    assert_int_equal(mofs_mkdir("/nowx", (mode_t)(MOFS_FTYPE_DIR | 0732U)), 0);
    assert_int_equal(mofs_mkdir("/nowx/child", (mode_t)(MOFS_FTYPE_DIR | 0777U)), 0);

    assert_int_equal(mofs_set_caller_user((uid_t)1001, (gid_t)1001, getpid()), 0);

    ret = mofs_create_core("/nowx/child/file.txt", 0644U, &inode_num);
    assert_int_equal(ret, MOFS_EACCES);
}

static void test_stat_denied_on_unreadable_file(void **state)
{
    mofs_stat_t st;

    (void)state;

    mofs_filehandle_t *fh = NULL;

    assert_int_equal(mofs_set_caller_user((uid_t)0, (gid_t)0, getpid()), 0);
    fh = mofs_open("/secret.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(fh);
    assert_int_equal(mofs_close(fh), 0);
    assert_int_equal(set_path_mode_bits("/secret.txt", 0000U), 0);

    assert_int_equal(mofs_set_caller_user((uid_t)1001, (gid_t)1001, getpid()), 0);
    assert_int_equal(mofs_stat("/secret.txt", &st), -1);
    assert_int_equal(errno, EACCES);
}

static void test_opendir_denied_on_unreadable_directory(void **state)
{
    mofs_dirhandle_t *handle = NULL;

    (void)state;

    assert_int_equal(mofs_set_caller_user((uid_t)0, (gid_t)0, getpid()), 0);
    assert_int_equal(mofs_mkdir("/privdir", (mode_t)(MOFS_FTYPE_DIR | 0000U)), 0);

    assert_int_equal(mofs_set_caller_user((uid_t)1001, (gid_t)1001, getpid()), 0);
    handle = mofs_opendir("/privdir");
    assert_null(handle);
    assert_int_equal(errno, EACCES);
}

static void test_unlink_denied_without_parent_write(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;

    assert_int_equal(mofs_set_caller_user((uid_t)0, (gid_t)0, getpid()), 0);
    assert_int_equal(mofs_mkdir("/parent", (mode_t)(MOFS_FTYPE_DIR | 0777U)), 0);
    handle = mofs_open("/parent/target.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(set_path_mode_bits("/parent", 0755U), 0);

    assert_int_equal(mofs_set_caller_user((uid_t)1001, (gid_t)1001, getpid()), 0);
    assert_int_equal(mofs_unlink("/parent/target.txt"), -1);
    assert_int_equal(errno, EACCES);
}

static void test_read_denied_for_other_user_without_permission(void **state)
{
    unsigned char      data[] = "secret";
    mofs_filehandle_t *handle = NULL;
    char               buf[16];
    ssize_t            nread;

    (void)state;

    assert_int_equal(mofs_set_caller_user((uid_t)0, (gid_t)0, getpid()), 0);
    handle = mofs_open("/owned.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0600U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, data, sizeof(data) - 1U), (ssize_t)(sizeof(data) - 1U));

    assert_int_equal(mofs_set_caller_user((uid_t)1001, (gid_t)1001, getpid()), 0);
    nread = mofs_read(handle, buf, sizeof(buf));
    assert_int_equal(nread, -1);
    assert_int_equal(errno, EACCES);

    assert_int_equal(mofs_set_caller_user((uid_t)0, (gid_t)0, getpid()), 0);
    assert_int_equal(mofs_close(handle), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_traverse_denied_without_execute_on_parent, setup_permissions_fixture,
                                        teardown_permissions_fixture),
        cmocka_unit_test_setup_teardown(test_create_denied_without_search_on_parent, setup_permissions_fixture,
                                        teardown_permissions_fixture),
        cmocka_unit_test_setup_teardown(test_stat_denied_on_unreadable_file, setup_permissions_fixture,
                                        teardown_permissions_fixture),
        cmocka_unit_test_setup_teardown(test_opendir_denied_on_unreadable_directory, setup_permissions_fixture,
                                        teardown_permissions_fixture),
        cmocka_unit_test_setup_teardown(test_unlink_denied_without_parent_write, setup_permissions_fixture,
                                        teardown_permissions_fixture),
        cmocka_unit_test_setup_teardown(test_read_denied_for_other_user_without_permission, setup_permissions_fixture,
                                        teardown_permissions_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
