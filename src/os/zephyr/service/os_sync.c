#include <mofs_errno.h>
#include <mofs_port_sync.h>

/* Zephyr stub: no-op mutex / core sync (later: k_mutex). */

struct mofs_mutex
{
    int unused;
};

static struct mofs_mutex stub_mutex;

int mofs_mutex_init(mofs_mutex_t **mutex)
{
    if (mutex == NULL) {
        return MOFS_EINVAL;
    }
    *mutex = &stub_mutex;
    return 0;
}

void mofs_mutex_fini(mofs_mutex_t *mutex)
{
    (void)mutex;
}

int mofs_mutex_lock(mofs_mutex_t *mutex)
{
    (void)mutex;
    return 0;
}

int mofs_mutex_unlock(mofs_mutex_t *mutex)
{
    (void)mutex;
    return 0;
}

int mofs_core_sync_init(void)
{
    return 0;
}

void mofs_core_sync_fini(void)
{
}

void mofs_core_sync_lock(void)
{
}

void mofs_core_sync_unlock(void)
{
}
