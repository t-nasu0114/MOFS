#include <mofs_port_mem.h>
#include <pthread.h>

static pthread_key_t  mofs_errno_key;
static pthread_once_t mofs_errno_once = PTHREAD_ONCE_INIT;

static void mofs_errno_key_destructor(void *ptr)
{
    mofs_free(ptr);
}

static void mofs_errno_key_init(void)
{
    (void)pthread_key_create(&mofs_errno_key, mofs_errno_key_destructor);
}

/**
 * @brief Return a pointer to the per-thread MOFS errno slot.
 *
 * Function behavior:
 * - Lazily allocates a thread-local `int` on first access via `pthread_key_t`.
 * - Initializes the slot to 0 when newly allocated.
 * - Returns NULL when heap allocation for the slot fails.
 *
 * @return Pointer to the current thread's errno slot on success.
 * @return NULL on allocation failure.
 */
int *mofs_errno_location(void)
{
    int *slot = NULL;

    (void)pthread_once(&mofs_errno_once, mofs_errno_key_init);
    slot = (int *)pthread_getspecific(mofs_errno_key);
    if (slot == NULL) {
        slot = (int *)mofs_malloc(sizeof(int));
        if (slot == NULL) {
            return NULL;
        }
        *slot = 0;
        (void)pthread_setspecific(mofs_errno_key, slot);
    }

    return slot;
}
