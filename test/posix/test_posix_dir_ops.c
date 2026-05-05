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

static int create_empty_file(const char *path)
{
    mofs_filehandle_t *handle = NULL;

    handle = mofs_open(path, MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    if (handle == NULL) {
        return -1;
    }
    return mofs_close(handle);
}

static int setup_posix_dir_fixture(void **state)
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
    ret = mofs_init_core(image_path, false, 0U, 0U);
    if (ret != 0) {
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }
    if (mofs_mkdir("/dir", 0755U) != 0) {
        (void)mofs_fini_core();
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }
    if (mofs_mkdir("/empty", 0755U) != 0) {
        (void)mofs_fini_core();
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }
    if (create_empty_file("/dir/item.txt") != 0) {
        (void)mofs_fini_core();
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }

    *state = image_path;
    return 0;
}

static int teardown_posix_dir_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

/* TC-P0-012: opendir on existing directory succeeds. */
static void test_TC_P0_012_opendir_success(void **state)
{
    mofs_dirhandle_t *handle = NULL;

    (void)state;
    errno  = 0;
    handle = mofs_opendir("/dir");
    assert_non_null(handle);
    assert_int_equal(errno, 0);
    assert_int_equal(mofs_closedir(handle), 0);
}

/* TC-P0-013: opendir on regular file fails with ENOTDIR. */
static void test_TC_P0_013_opendir_not_directory(void **state)
{
    mofs_dirhandle_t *handle = NULL;

    (void)state;
    errno  = 0;
    handle = mofs_opendir("/dir/item.txt");
    assert_null(handle);
    assert_int_equal(errno, ENOTDIR);
}

/* TC-P0-014: opendir on missing path fails with ENOENT. */
static void test_TC_P0_014_opendir_missing_path(void **state)
{
    mofs_dirhandle_t *handle = NULL;

    (void)state;
    errno  = 0;
    handle = mofs_opendir("/missing");
    assert_null(handle);
    assert_int_equal(errno, ENOENT);
}

/* TC-P0-015: readdir returns at least one valid entry. */
static void test_TC_P0_015_readdir_success(void **state)
{
    mofs_dirhandle_t *handle = NULL;
    mofs_dirent_t    *entry  = NULL;

    (void)state;
    handle = mofs_opendir("/dir");
    assert_non_null(handle);

    entry = mofs_readdir(handle);
    assert_non_null(entry);
    assert_true(entry->name[0] != '\0');
    assert_int_equal(mofs_closedir(handle), 0);
}

/* TC-P0-016: readdir at EOF returns NULL without changing errno. */
static void test_TC_P0_016_readdir_eof(void **state)
{
    mofs_dirhandle_t *handle = NULL;
    mofs_dirent_t    *entry  = NULL;
    int               guard  = 0;

    (void)state;
    handle = mofs_opendir("/dir");
    assert_non_null(handle);

    do {
        entry = mofs_readdir(handle);
        guard++;
    } while ((entry != NULL) && (guard < 8));

    errno = 0;
    entry = mofs_readdir(handle);
    assert_null(entry);
    assert_int_equal(errno, 0);
    assert_int_equal(mofs_closedir(handle), 0);
}

/* TC-P0-017: closedir on valid handle succeeds. */
static void test_TC_P0_017_closedir_success(void **state)
{
    mofs_dirhandle_t *handle = NULL;

    (void)state;
    handle = mofs_opendir("/dir");
    assert_non_null(handle);
    assert_int_equal(mofs_closedir(handle), 0);
}

/* TC-P0-018: closedir with invalid handle fails and sets errno. */
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
        cmocka_unit_test_setup_teardown(test_TC_P0_012_opendir_success, setup_posix_dir_fixture, teardown_posix_dir_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_013_opendir_not_directory, setup_posix_dir_fixture,
                                        teardown_posix_dir_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_014_opendir_missing_path, setup_posix_dir_fixture,
                                        teardown_posix_dir_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_015_readdir_success, setup_posix_dir_fixture, teardown_posix_dir_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_016_readdir_eof, setup_posix_dir_fixture, teardown_posix_dir_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_017_closedir_success, setup_posix_dir_fixture, teardown_posix_dir_fixture),
        cmocka_unit_test(test_TC_P0_018_closedir_invalid_handle),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
