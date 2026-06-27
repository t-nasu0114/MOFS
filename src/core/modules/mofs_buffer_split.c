/*
 * mofs_buffer_split.c — split-pool write-back block buffer cache
 *
 * Two independent LRU pools share a single mutex:
 *   meta_pool[MOFS_META_CACHE_NUM] — superblock, bitmaps, inode table
 *                                    (any block with blk_num < data_region_start)
 *   data_pool[MOFS_DATA_CACHE_NUM] — file-data blocks and list-node blocks
 *
 * The external API is identical to mofs_buffer.c so the rest of the
 * codebase can use either implementation without modification.
 *
 * Build selection (set by src/core/CMakeLists.txt):
 *   MOFS_BCACHE_IMPL=split  → this file is compiled, mofs_buffer.c is not
 *   MOFS_BCACHE_IMPL=unified (default) → mofs_buffer.c is compiled
 */

#include "mofs_block.h"
#include <mofs_buffer.h>
#include <mofs_config.h>
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_log.h>
#include <mofs_port_mem.h>
#include <mofs_port_sync.h>
#include <mofs_types.h>

#if !MOFS_BUFFER_CACHE_ENABLE

int mofs_bcache_init(void)
{
    return 0;
}

void mofs_bcache_fini(void)
{
}

int mofs_bcache_read_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                            unsigned int *read_blk_num, mofs_size_t *fraction)
{
    return read_continuous_blocks_raw(fd, buf, req_blk_num, start_blk_num, read_blk_num, fraction);
}

int mofs_bcache_write_blocks(int fd, const void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                             unsigned int *written_blk_num, mofs_size_t *fraction)
{
    return write_continuous_blocks_raw(fd, buf, req_blk_num, start_blk_num, written_blk_num, fraction);
}

int mofs_bcache_modify_block(unsigned int blk_num, mofs_size_t byte_off, const void *patch, mofs_size_t patch_len)
{
    (void)blk_num;
    (void)byte_off;
    (void)patch;
    (void)patch_len;
    return MOFS_EINVAL;
}

int mofs_bcache_flush(void)
{
    return 0;
}

int mofs_bcache_invalidate(unsigned int blk_num)
{
    (void)blk_num;
    return 0;
}

#else /* MOFS_BUFFER_CACHE_ENABLE */

/* -------------------------------------------------------------------------
 * Internal types and pool state
 * ---------------------------------------------------------------------- */

typedef struct bcache_entry
{
    mofs_bool     valid;    /* slot holds a valid block */
    mofs_bool     dirty;    /* contents differ from device (write-back) */
    unsigned int  blk_num;  /* absolute device block number */
    mofs_uint64_t lru_tick; /* last-access tick for LRU victim selection */
    void         *data;     /* one logical block (`ctx.sp_blk.blk_size` bytes) */
} bcache_entry_t;

/* Metadata pool: superblock, inode/data bitmaps, inode table */
static bcache_entry_t meta_pool[MOFS_META_CACHE_NUM];
static mofs_uint64_t  meta_tick = 0U;

/* Data pool: file-data blocks and list-node blocks */
static bcache_entry_t data_pool[MOFS_DATA_CACHE_NUM];
static mofs_uint64_t  data_tick = 0U;

static mofs_bool     bcache_ready = MOFS_FALSE;
static mofs_mutex_t *bcache_mutex = NULL;

/* -------------------------------------------------------------------------
 * Helpers: pool classification, lock, touch, find, flush-entry, get-victim
 * ---------------------------------------------------------------------- */

/**
 * @brief Decide which pool owns a block.
 *
 * @return MOFS_TRUE  → meta_pool (blk_num is below the data region)
 * @return MOFS_FALSE → data_pool
 */
static mofs_bool is_metadata_block(unsigned int blk_num)
{
    return (blk_num < ctx.sp_blk.data_region_start) ? MOFS_TRUE : MOFS_FALSE;
}

static void bcache_lock(void)
{
    (void)mofs_mutex_lock(bcache_mutex);
}

static void bcache_unlock(void)
{
    (void)mofs_mutex_unlock(bcache_mutex);
}

/**
 * @brief Stamp a slot with the current access tick for its pool.
 *
 * @param[in] pool     Array of pool entries.
 * @param[in] pool_num Pool size.
 * @param[in] tick_ref Pointer to the pool-specific tick counter.
 * @param[in] idx      Slot index to stamp.
 */
static void pool_touch(bcache_entry_t *pool, unsigned int pool_num, mofs_uint64_t *tick_ref, int idx)
{
    (void)pool_num;
    *tick_ref += 1U;
    pool[idx].lru_tick = *tick_ref;
}

/**
 * @brief Look up a block number in a pool.
 *
 * @return Pool index on hit, -1 on miss.
 */
static int pool_find(bcache_entry_t *pool, unsigned int pool_num, unsigned int blk_num)
{
    for (int i = 0; i < (int)pool_num; i++) {
        if ((pool[i].valid == MOFS_TRUE) && (pool[i].blk_num == blk_num)) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Write one dirty slot back to the device.
 *
 * @param[in] pool Pool that owns the slot.
 * @param[in] idx  Slot index to flush.
 * @return 0 on success, non-zero on I/O error.
 */
static int pool_flush_entry(bcache_entry_t *pool, int idx)
{
    unsigned int written_blk_num = 0U;
    mofs_size_t  fraction        = 0U;
    int          ret;

    ret = write_continuous_blocks_raw(ctx.dev_fd, pool[idx].data, 1U, pool[idx].blk_num,
                                      &written_blk_num, &fraction);
    if (ret != 0) {
        return ret;
    }
    if ((written_blk_num != 1U) || (fraction != 0U)) {
        return MOFS_EIO;
    }

    pool[idx].dirty = MOFS_FALSE;
    return 0;
}

/**
 * @brief Evict the LRU slot from a pool and return its index.
 *
 * Prefers an invalid (empty) slot; otherwise flushes the dirty LRU entry.
 *
 * @param[in] pool     Pool array.
 * @param[in] pool_num Pool size.
 * @param[out] out_idx Selected slot index.
 * @return 0 on success, non-zero on error.
 */
static int pool_get_victim(bcache_entry_t *pool, unsigned int pool_num, int *out_idx)
{
    int           idx  = -1;
    mofs_uint64_t best = MOFS_UINT64_MAX;
    int           ret  = 0;

    /* Prefer an empty slot. */
    for (int i = 0; i < (int)pool_num; i++) {
        if (pool[i].valid == MOFS_FALSE) {
            idx = i;
            break;
        }
    }

    /* Otherwise evict the least recently used slot. */
    if (idx < 0) {
        for (int i = 0; i < (int)pool_num; i++) {
            if (pool[i].lru_tick < best) {
                best = pool[i].lru_tick;
                idx  = i;
            }
        }
    }

    if (idx < 0) {
        return MOFS_EIO;
    }

    /* Persist dirty contents before reuse. */
    if ((pool[idx].valid == MOFS_TRUE) && (pool[idx].dirty == MOFS_TRUE)) {
        ret = pool_flush_entry(pool, idx);
        if (ret != 0) {
            return ret;
        }
    }

    pool[idx].valid = MOFS_FALSE;
    pool[idx].dirty = MOFS_FALSE;
    *out_idx        = idx;
    return 0;
}

/**
 * @brief Initialize all slots in a pool and allocate per-slot block storage.
 *
 * @param[in] pool     Pool array.
 * @param[in] pool_num Pool size.
 * @param[in] blk_sz   Bytes per block.
 * @return 0 on success; non-zero errno with partial allocations rolled back.
 */
static int pool_alloc(bcache_entry_t *pool, unsigned int pool_num, mofs_size_t blk_sz)
{
    mofs_memset(pool, 0, sizeof(bcache_entry_t) * pool_num);

    for (int i = 0; i < (int)pool_num; i++) {
        pool[i].data = mofs_malloc(blk_sz);
        if (pool[i].data == NULL) {
            int ret = get_errno();
            for (int j = 0; j < i; j++) {
                mofs_free(pool[j].data);
                pool[j].data = NULL;
            }
            return ret;
        }
        pool[i].valid    = MOFS_FALSE;
        pool[i].dirty    = MOFS_FALSE;
        pool[i].blk_num  = 0U;
        pool[i].lru_tick = 0U;
    }
    return 0;
}

/**
 * @brief Free all per-slot block storage in a pool and reset it.
 *
 * @param[in] pool     Pool array.
 * @param[in] pool_num Pool size.
 */
static void pool_free(bcache_entry_t *pool, unsigned int pool_num)
{
    for (int i = 0; i < (int)pool_num; i++) {
        if (pool[i].data != NULL) {
            mofs_free(pool[i].data);
            pool[i].data = NULL;
        }
        pool[i].valid = MOFS_FALSE;
        pool[i].dirty = MOFS_FALSE;
    }
}

/**
 * @brief Flush all dirty slots in a pool to the device.
 *
 * @param[in] pool     Pool array.
 * @param[in] pool_num Pool size.
 * @return 0 on success; first non-zero error encountered.
 */
static int pool_flush_all(bcache_entry_t *pool, unsigned int pool_num)
{
    int ret = 0;

    for (int i = 0; i < (int)pool_num; i++) {
        if ((pool[i].valid == MOFS_TRUE) && (pool[i].dirty == MOFS_TRUE)) {
            ret = pool_flush_entry(pool, i);
            if (ret != 0) {
                return ret;
            }
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Cache-miss load helper used by both read and modify paths
 * ---------------------------------------------------------------------- */

/**
 * @brief Load one block from the device into a pool slot.
 *
 * Selects an eviction victim, reads the block from the raw device, and
 * sets up the slot as valid/clean with the given block number.
 *
 * Caller must hold bcache_mutex.
 *
 * @param[in]  pool     Target pool array.
 * @param[in]  pool_num Pool size.
 * @param[in]  blk_num  Block to load.
 * @param[out] out_idx  Slot index that now holds the block.
 * @return 0 on success, non-zero on I/O or allocation error.
 */
static int pool_load_block(bcache_entry_t *pool, unsigned int pool_num, unsigned int blk_num, int *out_idx)
{
    unsigned int read_one = 0U;
    mofs_size_t  frac     = 0U;
    int          idx;
    int          ret;

    ret = pool_get_victim(pool, pool_num, &idx);
    if (ret != 0) {
        return ret;
    }

    ret = read_continuous_blocks_raw(ctx.dev_fd, pool[idx].data, 1U, blk_num, &read_one, &frac);
    if (ret != 0) {
        return ret;
    }
    if ((read_one != 1U) || (frac != 0U)) {
        return MOFS_EIO;
    }

    pool[idx].blk_num = blk_num;
    pool[idx].valid   = MOFS_TRUE;
    pool[idx].dirty   = MOFS_FALSE;
    *out_idx          = idx;
    return 0;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * @brief Initialize both cache pools.
 *
 * Requires `ctx.sp_blk.blk_size` to be set (call after superblock load).
 *
 * @return 0 on success (also when already initialized).
 * @return MOFS_EINVAL when the logical block size is unknown.
 * @return Non-zero errno value on allocation failure.
 */
int mofs_bcache_init(void)
{
    mofs_size_t blk_sz = (mofs_size_t)ctx.sp_blk.blk_size;
    int         ret    = 0;

    if (blk_sz == 0U) {
        return MOFS_EINVAL;
    }

    if (bcache_mutex == NULL) {
        ret = mofs_mutex_init(&bcache_mutex);
        if (ret != 0) {
            return ret;
        }
    }

    bcache_lock();
    if (bcache_ready == MOFS_TRUE) {
        bcache_unlock();
        return 0;
    }

    ret = pool_alloc(meta_pool, MOFS_META_CACHE_NUM, blk_sz);
    if (ret != 0) {
        bcache_unlock();
        return ret;
    }

    ret = pool_alloc(data_pool, MOFS_DATA_CACHE_NUM, blk_sz);
    if (ret != 0) {
        pool_free(meta_pool, MOFS_META_CACHE_NUM);
        bcache_unlock();
        return ret;
    }

    meta_tick    = 0U;
    data_tick    = 0U;
    bcache_ready = MOFS_TRUE;
    bcache_unlock();
    return 0;
}

/**
 * @brief Release both cache pools.
 *
 * Dirty entries are NOT flushed; callers must flush beforehand if needed.
 */
void mofs_bcache_fini(void)
{
    if (bcache_mutex == NULL) {
        return;
    }

    bcache_lock();
    pool_free(meta_pool, MOFS_META_CACHE_NUM);
    pool_free(data_pool, MOFS_DATA_CACHE_NUM);
    bcache_ready = MOFS_FALSE;
    bcache_unlock();

    mofs_mutex_fini(bcache_mutex);
    bcache_mutex = NULL;
}

/**
 * @brief Read contiguous blocks through the split-pool buffer cache.
 *
 * Each block is routed to the metadata or data pool based on its absolute
 * block number.  The cache-miss path loads the block via raw I/O and stores
 * it in the appropriate pool.
 *
 * @param[in]  fd            Device file descriptor.
 * @param[out] buf           Destination buffer.
 * @param[in]  req_blk_num   Number of blocks requested.
 * @param[in]  start_blk_num Absolute starting block number.
 * @param[out] read_blk_num  Number of full blocks served.
 * @param[out] fraction      Valid byte count for a trailing short read.
 * @return 0 on success (including short-read case; see `fraction`).
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EIO on an unexpected short read.
 * @return Non-zero errno propagated from raw block I/O.
 */
int mofs_bcache_read_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                            unsigned int *read_blk_num, mofs_size_t *fraction)
{
    int         ret       = 0;
    mofs_size_t blk_bytes = (mofs_size_t)ctx.sp_blk.blk_size;

    if ((fd < 0) || (buf == NULL) || (read_blk_num == NULL) || (fraction == NULL)) {
        return MOFS_EINVAL;
    }
    if (blk_bytes == 0U) {
        return MOFS_EINVAL;
    }
    if (bcache_ready != MOFS_TRUE) {
        return read_continuous_blocks_raw(fd, buf, req_blk_num, start_blk_num, read_blk_num, fraction);
    }

    bcache_lock();

    *fraction     = 0U;
    *read_blk_num = 0U;

    for (unsigned int i = 0U; i < req_blk_num; i++) {
        unsigned int    blk      = start_blk_num + i;
        mofs_bool       is_meta  = is_metadata_block(blk);
        bcache_entry_t *pool     = is_meta ? meta_pool : data_pool;
        unsigned int    pool_num = is_meta ? MOFS_META_CACHE_NUM : MOFS_DATA_CACHE_NUM;
        mofs_uint64_t  *tick_ref = is_meta ? &meta_tick : &data_tick;
        int             idx      = pool_find(pool, pool_num, blk);

        if (idx >= 0) {
            /* Cache hit: serve from memory. */
            mofs_memcpy(buf, pool[idx].data, blk_bytes);
            pool_touch(pool, pool_num, tick_ref, idx);
        } else {
            /* Cache miss: load from device. */
            unsigned int read_one = 0U;
            mofs_size_t  frac     = 0U;
            int          victim;

            ret = pool_get_victim(pool, pool_num, &victim);
            if (ret != 0) {
                break;
            }
            ret = read_continuous_blocks_raw(ctx.dev_fd, pool[victim].data, 1U, blk, &read_one, &frac);
            if (ret != 0) {
                break;
            }
            if (frac != 0U) {
                /* Device-tail short read: return partial bytes, do not cache. */
                *fraction = frac;
                mofs_memcpy(buf, pool[victim].data, frac);
                break;
            }
            if (read_one != 1U) {
                ret = MOFS_EIO;
                break;
            }

            pool[victim].blk_num = blk;
            pool[victim].valid   = MOFS_TRUE;
            pool[victim].dirty   = MOFS_FALSE;
            pool_touch(pool, pool_num, tick_ref, victim);
            mofs_memcpy(buf, pool[victim].data, blk_bytes);
        }

        *read_blk_num = *read_blk_num + 1U;
        buf           = (char *)buf + blk_bytes;
    }

    bcache_unlock();
    return ret;
}

/**
 * @brief Write contiguous blocks through the split-pool buffer cache (write-back).
 *
 * Stores each full block in the appropriate pool and marks it dirty.
 * Dirty contents reach the device on eviction or via `mofs_bcache_flush()`.
 *
 * @param[in]  fd               Device file descriptor.
 * @param[in]  buf              Source buffer.
 * @param[in]  req_blk_num      Number of blocks requested.
 * @param[in]  start_blk_num    Absolute starting block number.
 * @param[out] written_blk_num  Number of full blocks accepted.
 * @param[out] fraction         Always 0 for the cached path (API parity).
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return Non-zero errno from dirty-slot eviction flush.
 */
int mofs_bcache_write_blocks(int fd, const void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                             unsigned int *written_blk_num, mofs_size_t *fraction)
{
    int         ret       = 0;
    mofs_size_t blk_bytes = (mofs_size_t)ctx.sp_blk.blk_size;

    if ((fd < 0) || (buf == NULL) || (written_blk_num == NULL) || (fraction == NULL)) {
        return MOFS_EINVAL;
    }
    if (blk_bytes == 0U) {
        return MOFS_EINVAL;
    }
    if (bcache_ready != MOFS_TRUE) {
        return write_continuous_blocks_raw(fd, buf, req_blk_num, start_blk_num, written_blk_num, fraction);
    }

    bcache_lock();

    *fraction        = 0U;
    *written_blk_num = 0U;

    for (unsigned int i = 0U; i < req_blk_num; i++) {
        unsigned int    blk      = start_blk_num + i;
        mofs_bool       is_meta  = is_metadata_block(blk);
        bcache_entry_t *pool     = is_meta ? meta_pool : data_pool;
        unsigned int    pool_num = is_meta ? MOFS_META_CACHE_NUM : MOFS_DATA_CACHE_NUM;
        mofs_uint64_t  *tick_ref = is_meta ? &meta_tick : &data_tick;
        int             idx      = pool_find(pool, pool_num, blk);

        if (idx < 0) {
            /* Allocate a slot for a not-yet-cached block. */
            ret = pool_get_victim(pool, pool_num, &idx);
            if (ret != 0) {
                break;
            }
            pool[idx].blk_num = blk;
            pool[idx].valid   = MOFS_TRUE;
        }

        mofs_memcpy(pool[idx].data, buf, blk_bytes);
        pool[idx].dirty = MOFS_TRUE;
        pool_touch(pool, pool_num, tick_ref, idx);

        *written_blk_num = *written_blk_num + 1U;
        buf              = (const char *)buf + blk_bytes;
    }

    bcache_unlock();
    return ret;
}

/**
 * @brief Patch one byte range inside a cached logical block atomically.
 *
 * Routes the patch to the metadata or data pool based on `blk_num`.
 * Loads the target block when it is not already cached.
 *
 * @param[in] blk_num   Absolute device block number to update.
 * @param[in] byte_off  Byte offset within the block.
 * @param[in] patch     Source bytes to copy.
 * @param[in] patch_len Number of bytes to copy.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid or the cache is not ready.
 * @return MOFS_EIO on an unexpected short read while loading the block.
 * @return Non-zero errno from eviction or raw block I/O.
 */
int mofs_bcache_modify_block(unsigned int blk_num, mofs_size_t byte_off, const void *patch, mofs_size_t patch_len)
{
    mofs_size_t     blk_bytes = (mofs_size_t)ctx.sp_blk.blk_size;
    mofs_bool       is_meta   = is_metadata_block(blk_num);
    bcache_entry_t *pool      = is_meta ? meta_pool : data_pool;
    unsigned int    pool_num  = is_meta ? MOFS_META_CACHE_NUM : MOFS_DATA_CACHE_NUM;
    mofs_uint64_t  *tick_ref  = is_meta ? &meta_tick : &data_tick;
    int             idx       = -1;
    int             ret       = 0;

    if ((patch == NULL) || (patch_len == 0U) || (blk_bytes == 0U)) {
        return MOFS_EINVAL;
    }
    if (bcache_ready != MOFS_TRUE) {
        return MOFS_EINVAL;
    }
    if ((byte_off > blk_bytes) || (patch_len > blk_bytes) || ((byte_off + patch_len) > blk_bytes)) {
        return MOFS_EINVAL;
    }

    bcache_lock();

    idx = pool_find(pool, pool_num, blk_num);
    if (idx < 0) {
        ret = pool_load_block(pool, pool_num, blk_num, &idx);
    }

    if (ret == 0) {
        mofs_memcpy((char *)pool[idx].data + byte_off, patch, patch_len);
        pool[idx].dirty = MOFS_TRUE;
        pool_touch(pool, pool_num, tick_ref, idx);
    }

    bcache_unlock();
    return ret;
}

/**
 * @brief Flush all dirty entries from both pools and sync the device.
 *
 * Metadata pool is flushed first, then the data pool.
 *
 * @return 0 on success (also when the cache is not initialized).
 * @return Non-zero errno from raw block I/O or device sync.
 */
int mofs_bcache_flush(void)
{
    int ret = 0;

    if (bcache_ready != MOFS_TRUE) {
        return 0;
    }

    bcache_lock();

    ret = pool_flush_all(meta_pool, MOFS_META_CACHE_NUM);
    if (ret == 0) {
        ret = pool_flush_all(data_pool, MOFS_DATA_CACHE_NUM);
    }

    if (ret == 0) {
        if (dev_fsync(ctx.dev_fd) != 0) {
            ret = get_errno();
        }
    }

    bcache_unlock();
    return ret;
}

/**
 * @brief Drop a cached data block without writing it back.
 *
 * Only the data pool is searched. Metadata blocks are never invalidated
 * because they are always brought back in through controlled write paths
 * and should not be silently discarded.
 *
 * @param[in] blk_num Absolute device block number to invalidate.
 * @return 0 always (no-op when the block is not cached or cache is unready).
 */
int mofs_bcache_invalidate(unsigned int blk_num)
{
    int idx;

    if (bcache_ready != MOFS_TRUE) {
        return 0;
    }

    bcache_lock();

    /* Metadata is never invalidated; only search the data pool. */
    idx = pool_find(data_pool, MOFS_DATA_CACHE_NUM, blk_num);
    if (idx >= 0) {
        data_pool[idx].valid = MOFS_FALSE;
        data_pool[idx].dirty = MOFS_FALSE;
    }

    bcache_unlock();
    return 0;
}

#endif /* MOFS_BUFFER_CACHE_ENABLE */
