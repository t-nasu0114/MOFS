#include <mofs_errno.h>
#include <mofs_port_time.h>
#include <zephyr/kernel.h>

/* Zephyr stub: no wall clock yet (later: k_uptime_get). */

int mofs_now(mofs_time_sec_t *now)
{
    if (now == NULL) {
        return MOFS_EINVAL;
    }
    *now = (mofs_time_sec_t)(k_uptime_get() / 1000);
    return 0;
}
