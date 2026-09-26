#include <posix/mofs_posix_errno.h>
#include <zephyr/kernel.h>

/* Zephyr stub: single global errno slot (no TLS yet). */

static Z_THREAD_LOCAL int mofs_errno_slot;

/**
 * @brief Return a pointer to the MOFS errno slot (stub).
 *
 * Function behavior:
 * - Returns a process-wide static `int` (not thread-local).
 *
 * @return Pointer to the errno slot.
 */
int *mofs_errno_location(void)
{
    return &mofs_errno_slot;
}
