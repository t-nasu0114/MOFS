#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_posix.h>
#include <mofs_user.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../fixtures/test_fixture.h"
#include "mofs_block.h"

extern int mofs_format(const char *device_file, int fs_size, int blk_size);

#define TEST_LIST_BLK_SIZE 512U
/* At 512B blocks, one list node holds 126 pointers; 130 data blocks need two nodes. */
#define TEST_CHAINED_LIST_DATA_BLOCKS 130U

static int setup_block_list_fixture(void **state)
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

static int setup_block_list_512_fixture(void **state)
{
    static char image_path[128];
    int         ret = 0;

    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 4U * 1024U * 1024U);
    if (ret != 0) {
        return -1;
    }
    ret = mofs_format(image_path, 0, (int)TEST_LIST_BLK_SIZE);
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

static int teardown_block_list_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

/* Exceeds legacy 12-block inline inode limit (48KiB at 4K blocks). */
static void test_file_more_than_twelve_blocks(void **state)
{
    mofs_filehandle_t *handle = NULL;
    unsigned char     *buf    = NULL;
    size_t             file_size;
    ssize_t            written;
    ssize_t            read_back;

    (void)state;
    file_size = 13U * (size_t)MOFS_BLK_SIZE;
    buf       = (unsigned char *)malloc(file_size);
    assert_non_null(buf);
    for (size_t i = 0U; i < file_size; i++) {
        buf[i] = (unsigned char)(i & 0xFFU);
    }

    handle = mofs_open("/big13.bin", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);

    written = mofs_write(handle, buf, file_size);
    assert_int_equal(written, (ssize_t)file_size);

    memset(buf, 0, file_size);
    read_back = mofs_pread(handle, buf, file_size, 0);
    assert_int_equal(read_back, (ssize_t)file_size);
    assert_int_equal(buf[0], 0);
    assert_int_equal(buf[file_size - 1U], (int)(file_size - 1U) & 0xFF);

    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/big13.bin"), 0);
    free(buf);
}

static void assert_chained_list_nodes(const char *path, unsigned int expected_nr_blocks, uint32_t blk_size)
{
    mofs_inode_t          inode;
    int                   inode_num = 0;
    unsigned char        *blk_buf   = NULL;
    mofs_data_list_hdr_t *hdr       = NULL;
    unsigned int          ptrs_cap  = mofs_list_ptrs_per_node(blk_size);
    unsigned int          node_abs  = 0U;
    unsigned int          nodes     = 0U;
    unsigned int          read_blk_num;
    size_t                fraction;
    int                   ret;

    assert_true(ptrs_cap > 0U);
    assert_true(expected_nr_blocks > ptrs_cap);

    ret = mofs_path_to_inode_num(path, &inode_num);
    assert_int_equal(ret, 0);
    ret = mofs_read_inode(inode_num, &inode);
    assert_int_equal(ret, 0);
    assert_int_equal(inode.i_nr_blocks, expected_nr_blocks);
    assert_true(inode.i_data_head != 0U);

    blk_buf = (unsigned char *)malloc((size_t)blk_size);
    assert_non_null(blk_buf);

    node_abs = inode.i_data_head;
    while (node_abs != 0U) {
        ret = read_continuous_blocks(ctx.dev_fd, blk_buf, 1U, node_abs, &read_blk_num, &fraction);
        assert_int_equal(ret, 0);
        assert_int_equal(read_blk_num, 1U);

        hdr = (mofs_data_list_hdr_t *)blk_buf;
        nodes++;
        if (hdr->next_abs == 0U) {
            assert_int_equal(hdr->nr_ptrs, expected_nr_blocks - ptrs_cap * (nodes - 1U));
            break;
        }
        assert_int_equal(hdr->nr_ptrs, ptrs_cap);
        node_abs = hdr->next_abs;
    }

    assert_int_equal(nodes, 2U);
    free(blk_buf);
}

static void test_file_chained_list_nodes_at_512b(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    unsigned char     *buf    = NULL;
    unsigned char      sample;
    size_t             file_size;
    ssize_t            written;
    ssize_t            read_back;
    const unsigned int nr_blocks = TEST_CHAINED_LIST_DATA_BLOCKS;

    (void)state;
    file_size = (size_t)nr_blocks * (size_t)TEST_LIST_BLK_SIZE;
    buf       = (unsigned char *)malloc(file_size);
    assert_non_null(buf);
    for (size_t i = 0U; i < file_size; i++) {
        buf[i] = (unsigned char)(i & 0xFFU);
    }

    handle = mofs_open("/chain130.bin", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);

    written = mofs_write(handle, buf, file_size);
    assert_int_equal(written, (ssize_t)file_size);

    read_back = mofs_pread(handle, &sample, 1U, (off_t)((126U * TEST_LIST_BLK_SIZE) + 10U));
    assert_int_equal(read_back, 1);
    assert_int_equal(sample, (int)((126U * TEST_LIST_BLK_SIZE + 10U) & 0xFFU));

    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/chain130.bin", &st), 0);
    assert_int_equal((int)st.st_size, (int)file_size);

    assert_chained_list_nodes("/chain130.bin", nr_blocks, TEST_LIST_BLK_SIZE);

    handle = mofs_open("/chain130.bin", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    memset(buf, 0, file_size);
    read_back = mofs_pread(handle, buf, file_size, 0);
    assert_int_equal(read_back, (ssize_t)file_size);
    assert_int_equal(buf[0], 0);
    assert_int_equal(buf[file_size - 1U], (int)(file_size - 1U) & 0xFF);

    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/chain130.bin"), 0);
    free(buf);
}

static void test_write_at_max_file_size_returns_efbig(void **state)
{
    mofs_filehandle_t *handle = NULL;
    uint64_t           max_bytes;
    unsigned char      byte    = 0x5AU;
    ssize_t            written;

    (void)state;
    max_bytes = mofs_max_file_bytes();

    handle = mofs_open("/atlimit.bin", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);

    written = mofs_pwrite(handle, &byte, 1U, (off_t)max_bytes);
    assert_int_equal(written, -1);
    assert_int_equal(mofs_errno, MOFS_EFBIG);

    assert_int_equal(mofs_close(handle), 0);
    assert_int_equal(mofs_unlink("/atlimit.bin"), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_file_more_than_twelve_blocks, setup_block_list_fixture,
                                        teardown_block_list_fixture),
        cmocka_unit_test_setup_teardown(test_file_chained_list_nodes_at_512b, setup_block_list_512_fixture,
                                        teardown_block_list_fixture),
        cmocka_unit_test_setup_teardown(test_write_at_max_file_size_returns_efbig, setup_block_list_fixture,
                                        teardown_block_list_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
