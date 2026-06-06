#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_posix.h>
#include <mofs_port_user.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../fixtures/test_fixture.h"

extern int mofs_format(const char *device_file, int fs_size, int blk_size);

static int setup_truncate_fixture(void **state)
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
    ret = mofs_set_caller_user((mofs_uid_t)0, (mofs_gid_t)0, getpid());
    if (ret != 0) {
        (void)mofs_test_remove_file(image_path);
        return -1;
    }
    ret = mofs_init_core(image_path, MOFS_FALSE, 0U, 0U);
    if (ret != 0) {
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }

    *state = image_path;
    return 0;
}

static int teardown_truncate_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

static mofs_filehandle_t *create_file_with_data(const char *path, const unsigned char *data, mofs_size_t size)
{
    mofs_filehandle_t *handle = NULL;
    ssize_t            written;

    handle = mofs_open(path, MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    if (handle == NULL) {
        return NULL;
    }
    written = mofs_write(handle, (void *)data, size);
    if (written != (ssize_t)size) {
        (void)mofs_close(handle);
        return NULL;
    }
    return handle;
}

static void test_shrink_to_zero(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_inode_t       inode;
    mofs_stat_t        st;
    int                inode_num = 0;
    int                ret;

    (void)state;
    handle = create_file_with_data("/zero.txt", (const unsigned char *)"hello", 5U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_truncate("/zero.txt", 0), 0);

    assert_int_equal(mofs_stat("/zero.txt", &st), 0);
    assert_int_equal(st.st_size, 0);

    ret = mofs_path_to_inode_num("/zero.txt", &inode_num);
    assert_int_equal(ret, 0);
    ret = mofs_read_inode(inode_num, &inode);
    assert_int_equal(ret, 0);
    assert_int_equal(inode.i_nr_blocks, 0U);
    assert_int_equal(inode.i_size, 0U);

    assert_int_equal(mofs_unlink("/zero.txt"), 0);
}

static void test_shrink_partial_block(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    unsigned char     *buf    = NULL;
    unsigned char      read_buf[256];
    mofs_size_t             file_size = (mofs_size_t)MOFS_BLK_SIZE;
    ssize_t            read_back;

    (void)state;
    buf = (unsigned char *)malloc(file_size);
    assert_non_null(buf);
    memset(buf, 0xAB, file_size);

    handle = create_file_with_data("/partial.bin", buf, file_size);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_truncate("/partial.bin", 100), 0);

    assert_int_equal(mofs_stat("/partial.bin", &st), 0);
    assert_int_equal(st.st_size, 100);

    handle = mofs_open("/partial.bin", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    read_back = mofs_pread(handle, read_buf, sizeof(read_buf), 0);
    assert_int_equal(read_back, 100);
    assert_int_equal(read_buf[0], 0xAB);
    assert_int_equal(read_buf[99], 0xAB);
    assert_int_equal(mofs_pread(handle, read_buf, 16, 100), 0);
    assert_int_equal(mofs_close(handle), 0);

    free(buf);
    assert_int_equal(mofs_unlink("/partial.bin"), 0);
}

static void test_shrink_multi_block(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    unsigned char     *buf       = NULL;
    unsigned char      read_buf[32];
    mofs_size_t             file_size = 3U * (mofs_size_t)MOFS_BLK_SIZE;
    ssize_t            read_back;

    (void)state;
    buf = (unsigned char *)malloc(file_size);
    assert_non_null(buf);
    for (mofs_size_t i = 0U; i < file_size; i++) {
        buf[i] = (unsigned char)(i & 0xFFU);
    }

    handle = create_file_with_data("/multi.bin", buf, file_size);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_truncate("/multi.bin", (mofs_off_t)MOFS_BLK_SIZE + 10), 0);

    assert_int_equal(mofs_stat("/multi.bin", &st), 0);
    assert_int_equal(st.st_size, (mofs_off_t)MOFS_BLK_SIZE + 10);

    handle = mofs_open("/multi.bin", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    read_back = mofs_pread(handle, read_buf, sizeof(read_buf), (mofs_off_t)MOFS_BLK_SIZE);
    assert_int_equal(read_back, 10);
    assert_int_equal(read_buf[0], (int)(MOFS_BLK_SIZE & 0xFFU));
    assert_int_equal(read_buf[9], (int)((MOFS_BLK_SIZE + 9U) & 0xFFU));
    assert_int_equal(mofs_close(handle), 0);

    free(buf);
    assert_int_equal(mofs_unlink("/multi.bin"), 0);
}

static void test_grow(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    unsigned char      read_buf[32];
    ssize_t            read_back;

    (void)state;
    handle = create_file_with_data("/grow.txt", (const unsigned char *)"abc", 3U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_truncate("/grow.txt", 8192), 0);

    assert_int_equal(mofs_stat("/grow.txt", &st), 0);
    assert_int_equal(st.st_size, 8192);

    handle = mofs_open("/grow.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    read_back = mofs_pread(handle, read_buf, 3, 0);
    assert_int_equal(read_back, 3);
    assert_memory_equal(read_buf, "abc", 3U);
    read_back = mofs_pread(handle, read_buf, 16, 3);
    assert_int_equal(read_back, 16);
    for (ssize_t i = 0; i < read_back; i++) {
        assert_int_equal(read_buf[i], 0);
    }
    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/grow.txt"), 0);
}

static void test_no_op(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    unsigned char      read_buf[8];
    ssize_t            read_back;

    (void)state;
    handle = create_file_with_data("/noop.txt", (const unsigned char *)"data", 4U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_truncate("/noop.txt", 4), 0);

    assert_int_equal(mofs_stat("/noop.txt", &st), 0);
    assert_int_equal(st.st_size, 4);

    handle = mofs_open("/noop.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    read_back = mofs_read(handle, read_buf, sizeof(read_buf));
    assert_int_equal(read_back, 4);
    assert_memory_equal(read_buf, "data", 4U);
    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/noop.txt"), 0);
}

static void test_efbig(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_uint64_t           max_bytes;
    int                ret;

    (void)state;
    handle = mofs_open("/efbig.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);

    max_bytes = mofs_max_file_bytes();
    ret       = mofs_truncate_core(handle->inode_num, (mofs_off_t)(max_bytes + 1U));
    assert_int_equal(ret, MOFS_EFBIG);

    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/efbig.txt"), 0);
}

static void test_eisdir(void **state)
{
    int ret;

    (void)state;
    assert_int_equal(mofs_mkdir("/truncdir", 0755U), 0);
    ret = mofs_truncate("/truncdir", 0);
    assert_int_equal(ret, -1);
    assert_int_equal(mofs_errno, MOFS_EISDIR);
    assert_int_equal(mofs_rmdir("/truncdir"), 0);
}

static void test_ftruncate_ebadf(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/rdonly.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);

    handle = mofs_open("/rdonly.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_ftruncate(handle, 0), -1);
    assert_int_equal(mofs_errno, MOFS_EBADF);
    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/rdonly.txt"), 0);
}

static void test_ftruncate_via_handle(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    unsigned char      read_buf[8];

    (void)state;
    handle = create_file_with_data("/ftrunc.txt", (const unsigned char *)"12345678", 8U);
    assert_non_null(handle);

    assert_int_equal(mofs_ftruncate(handle, 5), 0);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/ftrunc.txt", &st), 0);
    assert_int_equal(st.st_size, 5);

    handle = mofs_open("/ftrunc.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_read(handle, read_buf, sizeof(read_buf)), 5);
    assert_memory_equal(read_buf, "12345", 5U);
    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/ftrunc.txt"), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_shrink_to_zero, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_shrink_partial_block, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_shrink_multi_block, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_grow, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_no_op, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_efbig, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_eisdir, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_ftruncate_ebadf, setup_truncate_fixture, teardown_truncate_fixture),
        cmocka_unit_test_setup_teardown(test_ftruncate_via_handle, setup_truncate_fixture, teardown_truncate_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
