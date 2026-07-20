#ifndef __MOFS_PORT_MEM__
#define __MOFS_PORT_MEM__

#include <mofs_port_types.h>

/**
 * @file mofs_port_mem.h
 * @brief Contract for heap and byte-oriented memory operations.
 */

/**
 * @brief Allocate a contiguous memory region.
 *
 * Function behavior:
 * - Allocates at least `size` bytes with alignment suitable for MOFS objects.
 * - The returned storage remains owned by the caller until `mofs_free()`.
 * - The initial contents are unspecified.
 *
 * @param[in] size Number of bytes to allocate.
 * @return Pointer to allocated storage on success.
 * @return NULL when allocation fails.
 */
void *mofs_malloc(mofs_size_t size);

/**
 * @brief Release storage allocated by `mofs_malloc()`.
 *
 * @param[in] ptr Allocation to release; NULL is permitted and is a no-op.
 */
void mofs_free(void *ptr);

/**
 * @brief Copy bytes between non-overlapping memory regions.
 *
 * @param[out] dest Destination region with space for at least `n` bytes.
 * @param[in] src Source region containing at least `n` readable bytes.
 * @param[in] n Number of bytes to copy.
 * @return `dest`.
 *
 * @note Behavior is undefined when the source and destination regions overlap.
 */
void *mofs_memcpy(void *dest, const void *src, mofs_size_t n);

/**
 * @brief Fill a memory region with a byte value.
 *
 * @param[out] s Destination region with space for at least `n` bytes.
 * @param[in] c Value converted to an unsigned byte before filling.
 * @param[in] n Number of bytes to fill.
 * @return `s`.
 */
void *mofs_memset(void *s, int c, mofs_size_t n);

#endif /* __MOFS_PORT_MEM__ */
