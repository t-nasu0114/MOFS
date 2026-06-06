#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_posix.h>
#include <mofs_user.h>
#include <pthread.h>
#include <unistd.h>

#include "../fixtures/test_fixture.h"

extern int mofs_format(const char *device_file, int fs_size, int blk_size);

typedef struct posix_errno_tls_ctx
{
    pthread_barrier_t barrier;
    int               thread_a_errno;
    int               thread_b_errno;
    int               thread_b_stat_ret;
} posix_errno_tls_ctx_t;

static int setup_posix_errno_tls_fixture(void **state)
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

    *state = image_path;
    return 0;
}

static int teardown_posix_errno_tls_fixture(void **state)
{
    const char *image_path = (const char *)*state;

    (void)mofs_fini_core();
    (void)mofs_clear_caller_user();
    return mofs_test_remove_file(image_path);
}

static void *thread_a_open_missing(void *arg)
{
    posix_errno_tls_ctx_t *ctx    = (posix_errno_tls_ctx_t *)arg;
    mofs_filehandle_t     *handle = NULL;

    (void)pthread_barrier_wait(&ctx->barrier);

    mofs_errno = 0;
    handle     = mofs_open("/missing-on-thread-a.txt", MOFS_OFLAG_RDONLY, 0U);
    ctx->thread_a_errno = mofs_errno;
    assert_null(handle);

    return NULL;
}

static void *thread_b_stat_root(void *arg)
{
    posix_errno_tls_ctx_t *ctx = (posix_errno_tls_ctx_t *)arg;
    mofs_stat_t            st  = {0};

    (void)pthread_barrier_wait(&ctx->barrier);

    mofs_errno = 0;
    ctx->thread_b_stat_ret = mofs_stat("/", &st);
    ctx->thread_b_errno    = mofs_errno;

    return NULL;
}

/* TC-P0-019: mofs_errno is independent per thread. */
static void test_TC_P0_019_mofs_errno_thread_local(void **state)
{
    posix_errno_tls_ctx_t ctx = {0};
    pthread_t             thread_a;
    pthread_t             thread_b;
    int                   ret = 0;

    (void)state;

    ret = pthread_barrier_init(&ctx.barrier, NULL, 3U);
    assert_int_equal(ret, 0);

    mofs_errno = 0;

    ret = pthread_create(&thread_a, NULL, thread_a_open_missing, &ctx);
    assert_int_equal(ret, 0);
    ret = pthread_create(&thread_b, NULL, thread_b_stat_root, &ctx);
    assert_int_equal(ret, 0);

    (void)pthread_barrier_wait(&ctx.barrier);

    ret = pthread_join(thread_a, NULL);
    assert_int_equal(ret, 0);
    ret = pthread_join(thread_b, NULL);
    assert_int_equal(ret, 0);

    assert_int_equal(ctx.thread_a_errno, MOFS_ENOENT);
    assert_int_equal(ctx.thread_b_stat_ret, 0);
    assert_int_equal(ctx.thread_b_errno, 0);
    assert_int_equal(mofs_errno, 0);

    (void)pthread_barrier_destroy(&ctx.barrier);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_TC_P0_019_mofs_errno_thread_local, setup_posix_errno_tls_fixture,
                                        teardown_posix_errno_tls_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
