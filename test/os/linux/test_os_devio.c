#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_type.h>

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

static void test_TC_P2_001_dev_open_rdonly_success(void **state)
{
    const char *image_path = (const char *)*state;
    int         fd         = -1;

    fd = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY);
    assert_true(fd >= 0);

    if (fd >= 0) {
        dev_close(fd);
    }
}

static void test_TC_P2_002_dev_open_invalid_flag(void **state)
{
    (void)state;
    skip();
}

static void test_TC_P2_003_dev_get_size_regular_file(void **state)
{
    const char         *image_path = (const char *)*state;
    int                 fd         = -1;
    int                 err        = 0;
    unsigned long long  bytes      = 0;

    fd = dev_open(image_path, MOFS_IO_OPEN_FLAG_RDONLY);
    assert_true(fd >= 0);

    bytes = dev_get_size(fd, &err);
    assert_int_equal(err, 0);
    assert_int_equal((long long)bytes, 8192LL);

    dev_close(fd);
}

static void test_TC_P2_004_dev_get_size_invalid_fd(void **state)
{
    int                err   = 0;
    unsigned long long bytes = 0;

    (void)state;
    bytes = dev_get_size(-1, &err);
    assert_int_equal((long long)bytes, 0LL);
    assert_true(err != 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_TC_P2_001_dev_open_rdonly_success, setup_temp_image, teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_TC_P2_002_dev_open_invalid_flag, setup_temp_image, teardown_temp_image),
        cmocka_unit_test_setup_teardown(test_TC_P2_003_dev_get_size_regular_file, setup_temp_image, teardown_temp_image),
        cmocka_unit_test(test_TC_P2_004_dev_get_size_invalid_fd),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
