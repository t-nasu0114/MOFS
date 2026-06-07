#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_time.h>
#include <time.h>

/**
 * @brief Get the current wall-clock time in seconds since Unix epoch.
 *
 * Function behavior:
 * - Calls `time()` to obtain current seconds.
 * - Writes the result to `now` on success.
 *
 * @param[out] now Destination for current time in seconds.
 * @return 0 on success.
 * @return MOFS_EINVAL if `now` is NULL.
 * @return Non-zero errno value from `get_errno()` when `time()` fails.
 */
int mofs_now(mofs_time_sec_t *now)
{
    time_t t;

    if (now == NULL) {
        return MOFS_EINVAL;
    }

    t = time(NULL);
    if (t == (time_t)-1) {
        return get_errno();
    }

    *now = (mofs_time_sec_t)t;
    return 0;
}
