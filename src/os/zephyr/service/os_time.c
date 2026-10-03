#include <mofs_errno.h>
#include <mofs_port_time.h>
#include <zephyr/kernel.h>

/* Zephyr: no wall clock yet, so timestamps are boot-relative with millisecond resolution. */

int mofs_now(mofs_timespec_t *now)
{
    int64_t ms;

    if (now == NULL) {
        return MOFS_EINVAL;
    }
    ms           = k_uptime_get();
    now->tv_sec  = (mofs_time_sec_t)(ms / 1000);
    now->tv_nsec = (mofs_uint32_t)(ms % 1000) * 1000000U;
    return 0;
}
