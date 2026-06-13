#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_mem.h>
#include <mofs_port_sync.h>
#include <pthread.h>

struct mofs_mutex
{
    pthread_mutex_t inner;
};

/**
 * @brief Allocate and initialize a mutex.
 *
 * Function behavior:
 * - Allocates a `mofs_mutex_t` object and initializes the OS mutex inside it.
 *
 * @param[out] mutex Destination pointer for the new mutex handle.
 * @return 0 on success.
 * @return MOFS_EINVAL if `mutex` is NULL.
 * @return Non-zero errno value from allocation or OS mutex initialization.
 */
int mofs_mutex_init(mofs_mutex_t **mutex)
{
    mofs_mutex_t *created = NULL;

    if (mutex == NULL) {
        return MOFS_EINVAL;
    }

    created = (mofs_mutex_t *)mofs_malloc(sizeof(*created));
    if (created == NULL) {
        return get_errno();
    }

    if (pthread_mutex_init(&created->inner, NULL) != 0) {
        int err = get_errno();
        mofs_free(created);
        return err;
    }

    *mutex = created;
    return 0;
}

/**
 * @brief Destroy a mutex and release its storage.
 *
 * Function behavior:
 * - Destroys the OS mutex and frees the wrapper object.
 * - Safe to call with NULL (no-op).
 *
 * @param[in] mutex Mutex handle to destroy.
 */
void mofs_mutex_fini(mofs_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }

    (void)pthread_mutex_destroy(&mutex->inner);
    mofs_free(mutex);
}

/**
 * @brief Acquire a mutex.
 *
 * @param[in] mutex Mutex handle to lock.
 * @return 0 on success.
 * @return MOFS_EINVAL if `mutex` is NULL.
 * @return Non-zero errno value from the OS lock operation.
 */
int mofs_mutex_lock(mofs_mutex_t *mutex)
{
    if (mutex == NULL) {
        return MOFS_EINVAL;
    }
    if (pthread_mutex_lock(&mutex->inner) != 0) {
        return get_errno();
    }
    return 0;
}

/**
 * @brief Release a mutex.
 *
 * @param[in] mutex Mutex handle to unlock.
 * @return 0 on success.
 * @return MOFS_EINVAL if `mutex` is NULL.
 * @return Non-zero errno value from the OS unlock operation.
 */
int mofs_mutex_unlock(mofs_mutex_t *mutex)
{
    if (mutex == NULL) {
        return MOFS_EINVAL;
    }
    if (pthread_mutex_unlock(&mutex->inner) != 0) {
        return get_errno();
    }
    return 0;
}

static pthread_mutex_t core_sync_mutex;
static int             core_sync_ready = 0;

/**
 * @brief Initialize the global core filesystem serialization mutex.
 *
 * @return 0 on success.
 * @return Non-zero errno value on mutex initialization failure.
 */
int mofs_core_sync_init(void)
{
    pthread_mutexattr_t attr;

    if (core_sync_ready != 0) {
        return 0;
    }

    if (pthread_mutexattr_init(&attr) != 0) {
        return get_errno();
    }
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        int err = get_errno();
        (void)pthread_mutexattr_destroy(&attr);
        return err;
    }
    if (pthread_mutex_init(&core_sync_mutex, &attr) != 0) {
        int err = get_errno();
        (void)pthread_mutexattr_destroy(&attr);
        return err;
    }
    (void)pthread_mutexattr_destroy(&attr);
    core_sync_ready = 1;
    return 0;
}

/**
 * @brief Destroy the global core filesystem serialization mutex.
 */
void mofs_core_sync_fini(void)
{
    if (core_sync_ready != 0) {
        (void)pthread_mutex_destroy(&core_sync_mutex);
        core_sync_ready = 0;
    }
}

/**
 * @brief Lock core filesystem operations.
 */
void mofs_core_sync_lock(void)
{
    if (core_sync_ready != 0) {
        (void)pthread_mutex_lock(&core_sync_mutex);
    }
}

/**
 * @brief Unlock core filesystem operations.
 */
void mofs_core_sync_unlock(void)
{
    if (core_sync_ready != 0) {
        (void)pthread_mutex_unlock(&core_sync_mutex);
    }
}
