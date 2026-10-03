#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_file.h>
#include <mofs_posix.h>
#include <mofs_port_user.h>
#include <stdio.h>
#include <string.h>
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

static int setup_posix_file_fixture(void **state)
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
    if (create_empty_file("/existing.txt") != 0) {
        (void)mofs_fini_core();
        (void)mofs_clear_caller_user();
        (void)mofs_test_remove_file(image_path);
        return -1;
    }

    *state = image_path;
    return 0;
}

static int teardown_posix_file_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

/* TC-P0-001: open existing file with read-only flag. */
static void test_TC_P0_001_open_existing_rdonly(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    mofs_errno  = 0;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_errno, 0);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-002: open missing file should fail with ENOENT. */
static void test_TC_P0_002_open_missing_path(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    mofs_errno  = 0;
    handle = mofs_open("/missing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_null(handle);
    assert_int_equal(mofs_errno, MOFS_ENOENT);
}

/* TC-P0-003: open without access mode should fail with EINVAL. */
static void test_TC_P0_003_open_without_accmode(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    mofs_errno  = 0;
    handle = mofs_open("/existing.txt", 0, 0U);
    assert_null(handle);
    assert_int_equal(mofs_errno, MOFS_EINVAL);
}

/* TC-P0-004: open with CREAT on missing path should create file. */
static void test_TC_P0_004_open_with_creat_on_missing_path(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    mofs_errno  = 0;
    handle = mofs_open("/created.txt", MOFS_OFLAG_CREAT | MOFS_OFLAG_RDWR, 0644U);
    assert_non_null(handle);
    assert_int_equal(mofs_errno, 0);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-005: open regular file with DIRECTORY flag should fail with ENOTDIR. */
static void test_TC_P0_005_open_directory_flag_for_regular_file(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    mofs_errno  = 0;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY | MOFS_OFLAG_DIRECTORY, 0U);
    assert_null(handle);
    assert_int_equal(mofs_errno, MOFS_ENOTDIR);
}

/* TC-P0-006: write then read file content successfully. */
static void test_TC_P0_006_read_success(void **state)
{
    mofs_filehandle_t *write_handle = NULL;
    mofs_filehandle_t *read_handle  = NULL;
    const char        *payload      = "hello-mofs";
    char               buf[16]      = {0};
    int                read_n       = 0;

    (void)state;
    write_handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(write_handle);
    assert_int_equal(mofs_write(write_handle, payload, strlen(payload)), (int)strlen(payload));
    assert_int_equal(mofs_close(write_handle), 0);

    read_handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(read_handle);
    read_n = mofs_read(read_handle, buf, 4U);
    assert_int_equal(read_n, 4);
    assert_memory_equal(buf, "hell", 4U);
    assert_int_equal(mofs_close(read_handle), 0);
}

/* TC-P0-007: read with NULL handle should fail with EINVAL. */
static void test_TC_P0_007_read_with_null_handle(void **state)
{
    int ret = 0;

    (void)state;
    mofs_errno = 0;
    ret   = mofs_read(NULL, NULL, 1U);
    assert_int_equal(ret, -1);
    assert_int_equal(mofs_errno, MOFS_EINVAL);
}

/* TC-P0-008: read with zero size should fail with EINVAL. */
static void test_TC_P0_008_read_with_zero_size(void **state)
{
    int ret = 0;

    (void)state;
    mofs_errno = 0;
    ret   = mofs_read(NULL, NULL, 0U);
    assert_int_equal(ret, -1);
    assert_int_equal(mofs_errno, MOFS_EINVAL);
}

/* TC-P0-009: second read at EOF should return zero without mofs_errno update. */
static void test_TC_P0_009_read_at_eof(void **state)
{
    mofs_filehandle_t *write_handle = NULL;
    mofs_filehandle_t *read_handle  = NULL;
    char               buf[32]      = {0};
    int                read_n       = 0;
    const char        *payload      = "hello-mofs";

    (void)state;
    write_handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(write_handle);
    assert_int_equal(mofs_write(write_handle, payload, strlen(payload)), (int)strlen(payload));
    assert_int_equal(mofs_close(write_handle), 0);

    read_handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(read_handle);

    read_n = mofs_read(read_handle, buf, sizeof(buf));
    assert_int_equal(read_n, (int)strlen(payload));

    mofs_errno  = 0;
    read_n = mofs_read(read_handle, buf, sizeof(buf));
    assert_int_equal(read_n, 0);
    assert_int_equal(mofs_errno, 0);
    assert_int_equal(mofs_close(read_handle), 0);
}

/* TC-P0-010: close a valid opened file handle. */
static void test_TC_P0_010_close_success(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-011: close with invalid handle should fail and set mofs_errno. */
static void test_TC_P0_011_close_with_invalid_handle(void **state)
{
    int ret = 0;

    (void)state;
    mofs_errno = 0;
    ret   = mofs_close(NULL);
    assert_true(ret != 0);
    assert_true(mofs_errno != 0);
}

/* TC-P0-012: SEEK_SET / SEEK_CUR / SEEK_END return the expected offset. */
static void test_TC_P0_012_lseek_set_cur_end(void **state)
{
    mofs_filehandle_t *handle  = NULL;
    const char        *payload = "hello-mofs";

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, payload, strlen(payload)), (int)strlen(payload));

    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_SET), 0);
    assert_int_equal(mofs_lseek(handle, 4, MOFS_SEEK_SET), 4);
    assert_int_equal(mofs_lseek(handle, 2, MOFS_SEEK_CUR), 6);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_END), (mofs_off_t)strlen(payload));
    assert_int_equal(mofs_lseek(handle, -3, MOFS_SEEK_END), (mofs_off_t)strlen(payload) - 3);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-013: a negative resulting offset is rejected and the position stays. */
static void test_TC_P0_013_lseek_negative_rejected(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_lseek(handle, 5, MOFS_SEEK_SET), 5);

    mofs_errno = 0;
    assert_int_equal(mofs_lseek(handle, -1, MOFS_SEEK_SET), (mofs_off_t)-1);
    assert_int_equal(mofs_errno, MOFS_EINVAL);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_CUR), 5);

    mofs_errno = 0;
    assert_int_equal(mofs_lseek(handle, -6, MOFS_SEEK_CUR), (mofs_off_t)-1);
    assert_int_equal(mofs_errno, MOFS_EINVAL);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_CUR), 5);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-014: seeking past EOF succeeds. */
static void test_TC_P0_014_lseek_past_eof(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, "ab", 2U), 2);
    assert_int_equal(mofs_lseek(handle, 100, MOFS_SEEK_SET), 100);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_CUR), 100);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-015: read after seek returns the bytes at the new offset. */
static void test_TC_P0_015_read_after_lseek(void **state)
{
    mofs_filehandle_t *handle  = NULL;
    const char        *payload = "hello-mofs";
    char               buf[8]  = {0};

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, payload, strlen(payload)), (int)strlen(payload));
    assert_int_equal(mofs_lseek(handle, 6, MOFS_SEEK_SET), 6);
    assert_int_equal(mofs_read(handle, buf, 4U), 4);
    assert_memory_equal(buf, "mofs", 4U);
    assert_int_equal(mofs_close(handle), 0);
}

/* Return 1 when `needle` occurs anywhere in the raw image, 0 otherwise. */
static int raw_image_contains(const char *image_path, const char *needle)
{
    FILE          *fp;
    unsigned char  buf[4096];
    size_t         needle_len;
    size_t         filled = 0U;
    size_t         nread;

    needle_len = strlen(needle);
    if ((image_path == NULL) || (needle_len == 0U) || (needle_len >= sizeof(buf))) {
        return 0;
    }
    fp = fopen(image_path, "rb");
    if (fp == NULL) {
        return 0;
    }
    while ((nread = fread(buf + filled, 1U, sizeof(buf) - filled, fp)) > 0U) {
        size_t total = filled + nread;
        size_t i;

        for (i = 0U; (i + needle_len) <= total; i++) {
            if (memcmp(buf + i, needle, needle_len) == 0) {
                (void)fclose(fp);
                return 1;
            }
        }
        if (total >= needle_len) {
            memmove(buf, buf + total - (needle_len - 1U), needle_len - 1U);
            filled = needle_len - 1U;
        } else {
            filled = total;
        }
    }
    (void)fclose(fp);
    return 0;
}

/* TC-P0-016: fsync on an open handle succeeds. */
static void test_TC_P0_016_fsync_open_handle(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_fsync(handle), 0);
    assert_int_equal(mofs_close(handle), 0);
    mofs_errno = 0;
    assert_int_equal(mofs_fsync(NULL), -1);
    assert_int_equal(mofs_errno, MOFS_EINVAL);
}

/* TC-P0-017: O_TRUNC on a writable open sets the file size and offset to 0. */
static void test_TC_P0_017_open_trunc_clears_file(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    char               buf[8] = {0};

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, "hello", 5U), 5);
    assert_int_equal(mofs_close(handle), 0);

    mofs_errno = 0;
    handle     = mofs_open("/existing.txt", MOFS_OFLAG_RDWR | MOFS_OFLAG_TRUNC, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_errno, 0);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_CUR), 0);
    assert_int_equal(mofs_read(handle, buf, sizeof(buf)), 0);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/existing.txt", &st), 0);
    assert_int_equal(st.st_size, 0);
}

/* TC-P0-018: O_RDONLY|O_TRUNC fails with EINVAL and leaves the file unchanged. */
static void test_TC_P0_018_open_rdonly_trunc_einval(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;
    char               buf[8] = {0};

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, "hello", 5U), 5);
    assert_int_equal(mofs_close(handle), 0);

    mofs_errno = 0;
    handle     = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY | MOFS_OFLAG_TRUNC, 0U);
    assert_null(handle);
    assert_int_equal(mofs_errno, MOFS_EINVAL);

    assert_int_equal(mofs_stat("/existing.txt", &st), 0);
    assert_int_equal(st.st_size, 5);
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_read(handle, buf, sizeof(buf)), 5);
    assert_memory_equal(buf, "hello", 5U);
    assert_int_equal(mofs_close(handle), 0);
}

/* TC-P0-019: O_TRUNC on a directory fails with EISDIR. */
static void test_TC_P0_019_open_trunc_directory_eisdir(void **state)
{
    mofs_filehandle_t *handle = NULL;

    (void)state;
    assert_int_equal(mofs_mkdir("/truncdir", 0755U), 0);

    mofs_errno = 0;
    handle     = mofs_open("/truncdir", MOFS_OFLAG_RDWR | MOFS_OFLAG_TRUNC, 0U);
    assert_null(handle);
    assert_int_equal(mofs_errno, MOFS_EISDIR);
}

/* TC-P0-020: O_APPEND writes at EOF even after lseek, including a second handle. */
static void test_TC_P0_020_append_writes_at_eof(void **state)
{
    mofs_filehandle_t *handle  = NULL;
    mofs_filehandle_t *second  = NULL;
    mofs_filehandle_t *reader  = NULL;
    char               buf[8]  = {0};

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, "AAAA", 4U), 4);
    assert_int_equal(mofs_close(handle), 0);

    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR | MOFS_OFLAG_APPEND, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_SET), 0);
    assert_int_equal(mofs_write(handle, "BB", 2U), 2);
    assert_int_equal(mofs_lseek(handle, 0, MOFS_SEEK_CUR), 6);

    second = mofs_open("/existing.txt", MOFS_OFLAG_WRONLY | MOFS_OFLAG_APPEND, 0U);
    assert_non_null(second);
    assert_int_equal(mofs_write(second, "C", 1U), 1);
    assert_int_equal(mofs_close(second), 0);
    assert_int_equal(mofs_close(handle), 0);

    reader = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(reader);
    assert_int_equal(mofs_read(reader, buf, sizeof(buf)), 7);
    assert_memory_equal(buf, "AAAABBC", 7U);
    assert_int_equal(mofs_close(reader), 0);
}

/* TC-P0-021: mofs_pwrite ignores O_APPEND and writes at the given offset. */
static void test_TC_P0_021_pwrite_ignores_append(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_filehandle_t *reader = NULL;
    char               buf[8] = {0};

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, "AAAA", 4U), 4);
    assert_int_equal(mofs_close(handle), 0);

    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR | MOFS_OFLAG_APPEND, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_pwrite(handle, "ZZ", 2U, 1), 2);
    assert_int_equal(mofs_close(handle), 0);

    reader = mofs_open("/existing.txt", MOFS_OFLAG_RDONLY, 0U);
    assert_non_null(reader);
    assert_int_equal(mofs_read(reader, buf, sizeof(buf)), 4);
    assert_memory_equal(buf, "AZZA", 4U);
    assert_int_equal(mofs_close(reader), 0);
}

/*
 * TC-P0-022: O_SYNC write reaches the image before unmount.
 * Cache disabled: mofs_bcache_flush is a no-op because the device is opened with O_SYNC,
 * so a missing marker before flush cannot be observed. The case is compiled only when
 * the write-back cache is enabled.
 */
#if MOFS_BUFFER_CACHE_ENABLE
static void test_TC_P0_022_osync_write_reaches_image(void **state)
{
    mofs_filehandle_t *handle      = NULL;
    const char        *image_path  = (const char *)*state;
    const char        *cached_mark = "NOSYNC-MARKER-UNFLUSHED";
    const char        *synced_mark = "OSYNC-MARKER-FLUSHED!!";

    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, cached_mark, strlen(cached_mark)), (int)strlen(cached_mark));
    assert_int_equal(raw_image_contains(image_path, cached_mark), 0);
    assert_int_equal(mofs_close(handle), 0);

    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR | MOFS_OFLAG_SYNC, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, synced_mark, strlen(synced_mark)), (int)strlen(synced_mark));
    assert_int_equal(raw_image_contains(image_path, synced_mark), 1);
    assert_int_equal(mofs_close(handle), 0);
}
#endif

/* TC-P0-023: ftruncate on an O_SYNC handle succeeds and updates the size. */
static void test_TC_P0_023_fsync_ftruncate_updates_size(void **state)
{
    mofs_filehandle_t *handle = NULL;
    mofs_stat_t        st;

    (void)state;
    handle = mofs_open("/existing.txt", MOFS_OFLAG_RDWR | MOFS_OFLAG_SYNC, 0U);
    assert_non_null(handle);
    assert_int_equal(mofs_write(handle, "hello", 5U), 5);
    assert_int_equal(mofs_ftruncate(handle, 2), 0);
    assert_int_equal(mofs_close(handle), 0);

    assert_int_equal(mofs_stat("/existing.txt", &st), 0);
    assert_int_equal(st.st_size, 2);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_TC_P0_001_open_existing_rdonly, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_002_open_missing_path, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_003_open_without_accmode, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_004_open_with_creat_on_missing_path, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_005_open_directory_flag_for_regular_file, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_006_read_success, setup_posix_file_fixture, teardown_posix_file_fixture),
        cmocka_unit_test(test_TC_P0_007_read_with_null_handle),
        cmocka_unit_test(test_TC_P0_008_read_with_zero_size),
        cmocka_unit_test_setup_teardown(test_TC_P0_009_read_at_eof, setup_posix_file_fixture, teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_010_close_success, setup_posix_file_fixture, teardown_posix_file_fixture),
        cmocka_unit_test(test_TC_P0_011_close_with_invalid_handle),
        cmocka_unit_test_setup_teardown(test_TC_P0_012_lseek_set_cur_end, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_013_lseek_negative_rejected, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_014_lseek_past_eof, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_015_read_after_lseek, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_016_fsync_open_handle, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_017_open_trunc_clears_file, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_018_open_rdonly_trunc_einval, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_019_open_trunc_directory_eisdir, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_020_append_writes_at_eof, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
        cmocka_unit_test_setup_teardown(test_TC_P0_021_pwrite_ignores_append, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
#if MOFS_BUFFER_CACHE_ENABLE
        cmocka_unit_test_setup_teardown(test_TC_P0_022_osync_write_reaches_image, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
#endif
        cmocka_unit_test_setup_teardown(test_TC_P0_023_fsync_ftruncate_updates_size, setup_posix_file_fixture,
                                        teardown_posix_file_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
