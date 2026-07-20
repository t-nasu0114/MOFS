#ifndef __MOFS_PORT_SYNC__
#define __MOFS_PORT_SYNC__

#include <mofs_port_types.h>

/**
 * @file mofs_port_sync.h
 * @brief Contract for MOFS mutexes and filesystem-wide serialization.
 */

/**
 * @brief Opaque platform mutex handle.
 *
 * The concrete `struct mofs_mutex` is defined by each platform implementation
 * and must not be inspected outside that implementation.
 */
typedef struct mofs_mutex mofs_mutex_t;

/**
 * @brief Allocate and initialize a mutex.
 *
 * @param[out] mutex Destination for the initialized mutex handle.
 * @return 0 on success.
 * @return MOFS_EINVAL when `mutex` is NULL.
 * @return Another positive `MOFS_E*` value on allocation or OS failure.
 */
int mofs_mutex_init(mofs_mutex_t **mutex);

/**
 * @brief Destroy a mutex and release implementation-owned resources.
 *
 * @param[in] mutex Mutex returned by `mofs_mutex_init()`; NULL is a no-op.
 *
 * @pre No thread may own or wait on `mutex`.
 */
void mofs_mutex_fini(mofs_mutex_t *mutex);

/**
 * @brief Acquire a mutex, waiting until it becomes available.
 *
 * @param[in,out] mutex Initialized mutex handle.
 * @return 0 on success.
 * @return MOFS_EINVAL when `mutex` is NULL.
 * @return Another positive `MOFS_E*` value on OS failure.
 */
int mofs_mutex_lock(mofs_mutex_t *mutex);

/**
 * @brief Release a mutex owned by the caller.
 *
 * @param[in,out] mutex Initialized mutex handle.
 * @return 0 on success.
 * @return MOFS_EINVAL when `mutex` is NULL.
 * @return Another positive `MOFS_E*` value on OS failure.
 */
int mofs_mutex_unlock(mofs_mutex_t *mutex);

/**
 * @brief Initialize filesystem-wide operation serialization.
 *
 * Function behavior:
 * - Initializes one process-wide/platform-wide recursive lock.
 * - Permits repeated initialization without replacing a live lock.
 * - Supports nested locking because POSIX entry points can call inode
 *   read-modify-write paths that also take the core lock.
 *
 * @return 0 on success or when already initialized.
 * @return A positive `MOFS_E*` value on failure.
 */
int mofs_core_sync_init(void);

/**
 * @brief Finalize filesystem-wide operation serialization.
 *
 * This function is a no-op when core synchronization is not initialized.
 *
 * @pre No thread may hold the core synchronization lock.
 */
void mofs_core_sync_fini(void);

/**
 * @brief Acquire the recursive filesystem-wide serialization lock.
 *
 * The function has no effect before successful `mofs_core_sync_init()`.
 */
void mofs_core_sync_lock(void);

/**
 * @brief Release one recursion level of the core serialization lock.
 *
 * The function has no effect before successful `mofs_core_sync_init()`.
 */
void mofs_core_sync_unlock(void);

#endif /* __MOFS_PORT_SYNC__ */
