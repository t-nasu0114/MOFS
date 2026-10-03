#ifndef __MOFS_PORT_TIME__
#define __MOFS_PORT_TIME__

#include <mofs_port_types.h>

/**
 * @file mofs_port_time.h
 * @brief Contract for timestamps stored in MOFS metadata.
 */

/** Signed whole seconds in the platform time base (negative values precede the base). */
typedef mofs_int64_t mofs_time_sec_t;

/** Timestamp with nanosecond resolution. */
typedef struct mofs_timespec
{
    mofs_time_sec_t tv_sec;  /* Whole seconds */
    mofs_uint32_t   tv_nsec; /* Nanoseconds, 0..MOFS_NSEC_PER_SEC - 1 */
} mofs_timespec_t;

/** Number of nanoseconds in one second. */
#define MOFS_NSEC_PER_SEC 1000000000U

/** Reserved `tv_sec` value used when no valid timestamp is available. */
#define MOFS_TIME_INVALID MOFS_INT64_MIN

/**
 * @brief Obtain the current timestamp for filesystem metadata.
 *
 * Function behavior:
 * - Returns seconds and nanoseconds from the best time source available to the platform.
 * - A Unix epoch wall clock is preferred for persistent timestamps.
 * - A platform without a wall clock may use a documented boot-relative time
 *   base; that value must not decrease during one boot.
 * - `tv_nsec` is always below `MOFS_NSEC_PER_SEC`. Platforms with a coarser clock
 *   leave the lower digits zero.
 *
 * @param[out] now Destination for the current timestamp.
 * @return 0 on success.
 * @return MOFS_EINVAL when `now` is NULL.
 * @return Another positive `MOFS_E*` value when time acquisition fails.
 */
int mofs_now(mofs_timespec_t *now);

#endif /* __MOFS_PORT_TIME__ */
