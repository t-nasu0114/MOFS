#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_types.h>
#include <string.h>

#include "../../fixtures/test_fixture.h"

static int setup_temp_image(void **state)
{
    static char image_path[128];
    int         ret = 0;

    ret = mofs_test_create_temp_image(image_path, sizeof(image_path), 8192U);
    if (ret != 0) {
        return -1;
    }

    *state = image_path;
    return 0;
}

static int teardown_temp_image(void **state)
{
    char *image_path = (char *)*state;

    if (image_path != NULL) {
        (void)mofs_test_remove_file(image_path);
    }
    return 0;
}

/* TC-P2-001: dev_open succeeds with RDONLY on existing file. */
static void test_TC_P2_001_dev_open_rdonly_success(void **state)
{
    const char *image_path = (const char *)*state;
    int         fd         = -1;
    int         ret        = 0;

    ret = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY, &fd);
    assert_int_equal(ret, 0);
    assert_true(fd >= 0);

    if (fd >= 0) {
        assert_int_equal(dev_close(fd), 0);
    }
}

/* TC-P2-002: dev_open with NONE flag returns MOFS_EINVAL. */
static void test_TC_P2_002_dev_open_invalid_flag(void **state)
{
    const char *image_path = (const char *)*state;
    int         fd         = -1;
    int         ret        = 0;

    ret = dev_open(image_path, MOFS_IO_OPEN_FLAG_NONE, &fd);
    assert_int_equal(ret, MOFS_EINVAL);
    assert_int_equal(fd, -1);
}

/* TC-P2-003: dev_get_size returns regular file byte size. */
static void test_TC_P2_003_dev_get_size_regular_file(void **state)
{
    const char         *image_path = (const char *)*state;
    int                 fd         = -1;
    int                 ret        = 0;
    unsigned long long  bytes      = 0;

    ret = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY, &fd);
    assert_int_equal(ret, 0);
    assert_true(fd >= 0);

    ret = dev_get_size(fd, &bytes);
    assert_int_equal(ret, 0);
    assert_int_equal((long long)bytes, 8192LL);

    assert_int_equal(dev_close(fd), 0);
}

/* TC-P2-004: dev_get_size on invalid fd returns zero and sets err. */
static void test_TC_P2_004_dev_get_size_invalid_fd(void **state)
{
    int                ret   = 0;
    unsigned long long bytes = 0;

    (void)state;
    ret = dev_get_size(-1, &bytes);
    assert_int_equal(ret, MOFS_EBADF);
    assert_int_equal((long long)bytes, 0LL);
}

static void test_dev_open_missing_path_returns_mofs_enoent(void **state)
{
    int fd  = -1;
    int ret = 0;

    (void)state;
    ret = dev_open("/tmp/path_that_should_not_exist_mofs_devio", MOFS_IO_OPEN_FLAG_RDONLY, &fd);
    assert_int_equal(ret, MOFS_ENOENT);
    assert_int_equal(fd, -1);
}

static void test_devio_rejects_null_output_pointers(void **state)
{
    const char *image_path = (const char *)*state;
    char        byte       = 0;

    assert_int_equal(dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY, NULL), MOFS_EINVAL);
    assert_int_equal(dev_read(-1, &byte, 1U, NULL), MOFS_EINVAL);
    assert_int_equal(dev_write(-1, &byte, 1U, NULL), MOFS_EINVAL);
    assert_int_equal(dev_lseek(-1, 0, MOFS_SEEK_SET, NULL), MOFS_EINVAL);
    assert_int_equal(dev_get_size(-1, NULL), MOFS_EINVAL);
}

static void test_devio_write_seek_read_and_fsync(void **state)
{
    const char *image_path = (const char *)*state;
    const char  write_buf[] = "mofs-devio";
    char        read_buf[sizeof(write_buf)];
    int         fd         = -1;
    int         ret        = 0;
    mofs_size_t written    = 0U;
    mofs_size_t read_size  = 0U;
    mofs_off_t  new_offset = 0;

    memset(read_buf, 0, sizeof(read_buf));
    ret = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDWR, &fd);
    assert_int_equal(ret, 0);

    ret = dev_lseek(fd, 0, MOFS_SEEK_SET, &new_offset);
    assert_int_equal(ret, 0);
    assert_int_equal(new_offset, 0);

    ret = dev_write(fd, write_buf, (mofs_size_t)sizeof(write_buf), &written);
    assert_int_equal(ret, 0);
    assert_int_equal(written, sizeof(write_buf));
    assert_int_equal(dev_fsync(fd), 0);

    ret = dev_lseek(fd, 0, MOFS_SEEK_SET, &new_offset);
    assert_int_equal(ret, 0);
    ret = dev_read(fd, read_buf, (mofs_size_t)sizeof(read_buf), &read_size);
    assert_int_equal(ret, 0);
    assert_int_equal(read_size, sizeof(read_buf));
    assert_memory_equal(read_buf, write_buf, sizeof(write_buf));

    assert_int_equal(dev_close(fd), 0);
}

static void test_dev_read_reports_short_read_as_success(void **state)
{
    const char *image_path = (const char *)*state;
    char        read_buf[4] = {0};
    int         fd          = -1;
    int         ret         = 0;
    mofs_size_t read_size   = 0U;
    mofs_off_t  new_offset  = 0;

    ret = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY, &fd);
    assert_int_equal(ret, 0);
    ret = dev_lseek(fd, 8190, MOFS_SEEK_SET, &new_offset);
    assert_int_equal(ret, 0);
    assert_int_equal(new_offset, 8190);

    ret = dev_read(fd, read_buf, (mofs_size_t)sizeof(read_buf), &read_size);
    assert_int_equal(ret, 0);
    assert_int_equal(read_size, 2);
    assert_int_equal(dev_close(fd), 0);
}

static void test_dev_lseek_rejects_invalid_whence(void **state)
{
    const char *image_path = (const char *)*state;
    int         fd         = -1;
    int         ret        = 0;
    mofs_off_t  new_offset = 123;

    ret = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY, &fd);
    assert_int_equal(ret, 0);
    ret = dev_lseek(fd, 0, 999, &new_offset);
    assert_int_equal(ret, MOFS_EINVAL);
    assert_int_equal(new_offset, 0);
    assert_int_equal(dev_close(fd), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_TC_P2_001_dev_open_rdonly_success, setup_temp_image, teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_TC_P2_002_dev_open_invalid_flag, setup_temp_image, teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_TC_P2_003_dev_get_size_regular_file, setup_temp_image, teardown_temp_image),
        cmocka_unit_test(test_TC_P2_004_dev_get_size_invalid_fd),
        cmocka_unit_test(test_dev_open_missing_path_returns_mofs_enoent),
        cmocka_unit_test_setup_teardown(test_devio_rejects_null_output_pointers, setup_temp_image, teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_devio_write_seek_read_and_fsync, setup_temp_image, teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_dev_read_reports_short_read_as_success, setup_temp_image,
                                        teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_dev_lseek_rejects_invalid_whence, setup_temp_image, teardown_temp_image),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
