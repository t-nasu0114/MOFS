#ifndef __MOFS_PORT_TIME__
#define __MOFS_PORT_TIME__

#include <mofs_port_types.h>

/**
 * @file mofs_port_time.h
 * @brief Contract for timestamps stored in MOFS metadata.
 */

/** Timestamp value expressed in whole seconds in the platform time base. */
typedef mofs_uint64_t mofs_time_sec_t;

/** Reserved value used when no valid timestamp is available. */
#define MOFS_TIME_INVALID 0ULL

/**
 * @brief Obtain the current timestamp for filesystem metadata.
 *
 * Function behavior:
 * - Returns whole seconds from the best time source available to the platform.
 * - A Unix epoch wall clock is preferred for persistent timestamps.
 * - A platform without a wall clock may use a documented boot-relative time
 *   base; that value must not decrease during one boot.
 *
 * @param[out] now Destination for the current timestamp.
 * @return 0 on success.
 * @return MOFS_EINVAL when `now` is NULL.
 * @return Another positive `MOFS_E*` value when time acquisition fails.
 */
int mofs_now(mofs_time_sec_t *now);

#endif /* __MOFS_PORT_TIME__ */
