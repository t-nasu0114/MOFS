#define _POSIX_C_SOURCE 200809L

#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_time.h>
#include <time.h>

/**
 * @brief Get the current wall-clock time since Unix epoch.
 *
 * Function behavior:
 * - Calls `clock_gettime(CLOCK_REALTIME)` to obtain seconds and nanoseconds.
 * - Writes the result to `now` on success.
 *
 * @param[out] now Destination for the current time.
 * @return 0 on success.
 * @return MOFS_EINVAL if `now` is NULL.
 * @return Non-zero errno value from `get_errno()` when `clock_gettime()` fails.
 */
int mofs_now(mofs_timespec_t *now)
{
    struct timespec ts;

    if (now == NULL) {
        return MOFS_EINVAL;
    }

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return get_errno();
    }

    now->tv_sec  = (mofs_time_sec_t)ts.tv_sec;
    now->tv_nsec = (mofs_uint32_t)ts.tv_nsec;
    return 0;
}
