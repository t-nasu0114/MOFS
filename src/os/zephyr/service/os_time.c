#include <mofs_errno.h>
#include <mofs_port_time.h>

/* Zephyr stub: no wall clock yet (later: k_uptime_get). */

int mofs_now(mofs_time_sec_t *now)
{
    if (now == NULL) {
        return MOFS_EINVAL;
    }
    *now = MOFS_TIME_INVALID;
    return MOFS_ENOSYS;
}
