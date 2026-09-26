#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_sync.h>
#include <zephyr/kernel.h>

/* Zephyr stub: no-op mutex / core sync (later: k_mutex). */

struct mofs_mutex
{
    struct k_mutex inner;
};

static struct mofs_mutex zephyr_mutex;

int mofs_mutex_init(mofs_mutex_t **mutex)
{
    int ret = 0;
    if (mutex == NULL) {
        return MOFS_EINVAL;
    }
    *mutex = &zephyr_mutex;
    ret    = k_mutex_init(&((*mutex)->inner));
    if (ret != 0) {
        return get_errno();
    }
    return 0;
}

void mofs_mutex_fini(mofs_mutex_t *mutex)
{
    (void)mutex;
    /* Do nothing because zephyr mutex is kernel object */
}

int mofs_mutex_lock(mofs_mutex_t *mutex)
{
    int ret = 0;

    if (mutex == NULL) {
        return MOFS_EINVAL;
    }
    ret = k_mutex_lock(&(mutex->inner), K_FOREVER);
    if (ret != 0) {
        return get_errno();
    }
    return 0;
}

int mofs_mutex_unlock(mofs_mutex_t *mutex)
{
    int ret = 0;

    if (mutex == NULL) {
        return MOFS_EINVAL;
    }
    ret = k_mutex_unlock(&(mutex->inner));
    if (ret != 0) {
        return get_errno();
    }
    return 0;
}

static struct k_mutex zephyr_core_sync_mutex;
static bool           zephyr_core_sync_initialized = false;

int mofs_core_sync_init(void)
{
    int ret = 0;
    if (zephyr_core_sync_initialized) {
        return 0;
    }
    ret = k_mutex_init(&zephyr_core_sync_mutex);
    if (ret != 0) {
        return get_errno();
    }
    /* Zephyr mutex is recursive by default */

    zephyr_core_sync_initialized = true;
    return 0;
}

void mofs_core_sync_fini(void)
{
    /* Do nothing because zephyr mutex is kernel object */
}

void mofs_core_sync_lock(void)
{
    if (!zephyr_core_sync_initialized) {
        return;
    }
    k_mutex_lock(&zephyr_core_sync_mutex, K_FOREVER);
    return;
}

void mofs_core_sync_unlock(void)
{
    if (!zephyr_core_sync_initialized) {
        return;
    }
    k_mutex_unlock(&zephyr_core_sync_mutex);
    return;
}
