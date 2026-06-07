
#include "mofs_block.h"
#include <mofs_buffer.h>
#include <mofs_config.h>
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_log.h>
#include <mofs_port_mem.h>
#include <mofs_types.h>

/* One cached logical block keyed by its absolute device block number. */
typedef struct bcache_entry
{
    mofs_bool     valid;    /* slot holds a valid block */
    mofs_bool     dirty;    /* contents differ from device (write-back) */
    unsigned int  blk_num;  /* absolute device block number */
    mofs_uint64_t lru_tick; /* last-access tick for LRU victim selection */
    void         *data;     /* one logical block (`ctx.sp_blk.blk_size` bytes) */
} bcache_entry_t;

static bcache_entry_t bcache_pool[MOFS_BUFFER_CACHE_NUM];
static mofs_bool      bcache_ready = MOFS_FALSE;
static mofs_uint64_t  bcache_tick  = 0U;

/**
 * @brief Mark a cache slot as the most recently used entry.
 *
 * Function behavior:
 * - Increments the global access tick and stamps the slot with it.
 *
 * @param[in] idx Pool index to stamp.
 */
static void bcache_touch(int idx)
{
    bcache_tick += 1U;
    bcache_pool[idx].lru_tick = bcache_tick;
}

/**
 * @brief Find the cache slot that currently holds a block number.
 *
 * @param[in] blk_num Absolute device block number to look up.
 * @return Pool index when a valid slot holds the block.
 * @return -1 when the block is not cached.
 */
static int bcache_find(unsigned int blk_num)
{
    for (int i = 0; i < (int)MOFS_BUFFER_CACHE_NUM; i++) {
        if ((bcache_pool[i].valid == MOFS_TRUE) && (bcache_pool[i].blk_num == blk_num)) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Write one dirty cache slot back to the device.
 *
 * Function behavior:
 * - Uses the bypass (`*_raw`) block writer so the flush itself is not cached.
 * - Clears the dirty flag on success.
 *
 * @param[in] idx Pool index of the dirty slot to flush.
 * @return 0 on success.
 * @return MOFS_EIO on an unexpected short write.
 * @return Non-zero errno value propagated from raw block I/O.
 */
static int bcache_flush_entry(int idx)
{
    unsigned int written_blk_num = 0U;
    mofs_size_t  fraction        = 0U;
    int          ret;

    ret = write_continuous_blocks_raw(ctx.dev_fd, bcache_pool[idx].data, 1U, bcache_pool[idx].blk_num,
                                      &written_blk_num, &fraction);
    if (ret != 0) {
        return ret;
    }
    if ((written_blk_num != 1U) || (fraction != 0U)) {
        return MOFS_EIO;
    }

    bcache_pool[idx].dirty = MOFS_FALSE;
    return 0;
}

/**
 * @brief Pick a reusable cache slot and prepare it for a new block.
 *
 * Function behavior:
 * - Prefers an invalid (empty) slot.
 * - Otherwise evicts the least-recently-used slot, flushing it first when dirty.
 * - Resets the chosen slot to an invalid/clean state for the caller to fill.
 *
 * @param[out] out_idx Destination for the selected pool index.
 * @return 0 on success.
 * @return MOFS_EIO when no slot can be selected.
 * @return Non-zero errno value propagated from a dirty-slot flush.
 */
static int bcache_get_victim(int *out_idx)
{
    int           idx  = -1;
    mofs_uint64_t best = MOFS_UINT64_MAX;
    int           ret  = 0;

    /* Prefer an empty slot. */
    for (int i = 0; i < (int)MOFS_BUFFER_CACHE_NUM; i++) {
        if (bcache_pool[i].valid == MOFS_FALSE) {
            idx = i;
            break;
        }
    }

    /* Otherwise evict the least recently used slot. */
    if (idx < 0) {
        for (int i = 0; i < (int)MOFS_BUFFER_CACHE_NUM; i++) {
            if (bcache_pool[i].lru_tick < best) {
                best = bcache_pool[i].lru_tick;
                idx  = i;
            }
        }
    }

    if (idx < 0) {
        return MOFS_EIO;
    }

    /* Persist contents before reusing the slot. */
    if ((bcache_pool[idx].valid == MOFS_TRUE) && (bcache_pool[idx].dirty == MOFS_TRUE)) {
        ret = bcache_flush_entry(idx);
        if (ret != 0) {
            return ret;
        }
    }

    bcache_pool[idx].valid = MOFS_FALSE;
    bcache_pool[idx].dirty = MOFS_FALSE;
    *out_idx               = idx;
    return 0;
}

/**
 * @brief Allocate and initialize the block buffer cache pool.
 *
 * Function behavior:
 * - Requires `ctx.sp_blk.blk_size` to be set (call after superblock load).
 * - Allocates one logical block of backing storage per pool entry.
 * - Rolls back all allocations if any single allocation fails.
 *
 * @return 0 on success (also when already initialized).
 * @return MOFS_EINVAL when the logical block size is unknown.
 * @return Non-zero errno value from `get_errno()` on allocation failure.
 */
int mofs_bcache_init(void)
{
    mofs_size_t blk_sz = (mofs_size_t)ctx.sp_blk.blk_size;

    if (blk_sz == 0U) {
        return MOFS_EINVAL;
    }
    if (bcache_ready == MOFS_TRUE) {
        return 0;
    }

    mofs_memset(bcache_pool, 0, sizeof(bcache_pool));

    /* Allocate per-entry block storage. */
    for (int i = 0; i < (int)MOFS_BUFFER_CACHE_NUM; i++) {
        bcache_pool[i].data = mofs_malloc(blk_sz);
        if (bcache_pool[i].data == NULL) {
            int err = get_errno();
            /* Roll back allocations done so far. */
            for (int j = 0; j < i; j++) {
                mofs_free(bcache_pool[j].data);
                bcache_pool[j].data = NULL;
            }
            return err;
        }
        bcache_pool[i].valid    = MOFS_FALSE;
        bcache_pool[i].dirty    = MOFS_FALSE;
        bcache_pool[i].blk_num  = 0U;
        bcache_pool[i].lru_tick = 0U;
    }

    bcache_tick  = 0U;
    bcache_ready = MOFS_TRUE;
    return 0;
}

/**
 * @brief Release the block buffer cache pool.
 *
 * Function behavior:
 * - Frees all per-entry block storage and resets the cache to an unready state.
 * - Does NOT flush dirty entries; callers must flush beforehand if needed.
 */
void mofs_bcache_fini(void)
{
    for (int i = 0; i < (int)MOFS_BUFFER_CACHE_NUM; i++) {
        if (bcache_pool[i].data != NULL) {
            mofs_free(bcache_pool[i].data);
            bcache_pool[i].data = NULL;
        }
        bcache_pool[i].valid = MOFS_FALSE;
        bcache_pool[i].dirty = MOFS_FALSE;
    }
    bcache_ready = MOFS_FALSE;
}

/**
 * @brief Read contiguous blocks through the buffer cache.
 *
 * Function behavior:
 * - Serves cached blocks from memory; loads missing blocks via raw block I/O.
 * - Preserves the `read_continuous_blocks` semantics for short reads
 *   (a device-tail short read sets `fraction` and stops without counting).
 * - Falls back to raw block I/O when the cache is not initialized.
 *
 * @param[in] fd Device file descriptor.
 * @param[out] buf Destination buffer for contiguous blocks.
 * @param[in] req_blk_num Number of blocks requested.
 * @param[in] start_blk_num Absolute starting block number.
 * @param[out] read_blk_num Number of full blocks served.
 * @param[out] fraction Valid byte count for a trailing short read.
 * @return 0 on success (including short-read case; see `fraction`).
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EIO on an unexpected short read.
 * @return Non-zero errno value propagated from raw block I/O.
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

    *fraction     = 0U;
    *read_blk_num = 0U;

    for (unsigned int i = 0U; i < req_blk_num; i++) {
        unsigned int blk = start_blk_num + i;
        int          idx = bcache_find(blk);

        if (idx >= 0) {
            /* Cache hit: serve from memory. */
            mofs_memcpy(buf, bcache_pool[idx].data, blk_bytes);
            bcache_touch(idx);
        } else {
            /* Cache miss: load one block into a free/evicted slot. */
            unsigned int read_one = 0U;
            mofs_size_t  frac     = 0U;

            ret = bcache_get_victim(&idx);
            if (ret != 0) {
                break;
            }
            ret = read_continuous_blocks_raw(ctx.dev_fd, bcache_pool[idx].data, 1U, blk, &read_one, &frac);
            if (ret != 0) {
                break;
            }
            if (frac != 0U) {
                /* Device-tail short read: return partial bytes, do not cache. */
                *fraction = frac;
                mofs_memcpy(buf, bcache_pool[idx].data, frac);
                break;
            }
            if (read_one != 1U) {
                ret = MOFS_EIO;
                break;
            }

            bcache_pool[idx].blk_num = blk;
            bcache_pool[idx].valid   = MOFS_TRUE;
            bcache_pool[idx].dirty   = MOFS_FALSE;
            bcache_touch(idx);
            mofs_memcpy(buf, bcache_pool[idx].data, blk_bytes);
        }

        *read_blk_num = *read_blk_num + 1U;
        buf           = (char *)buf + blk_bytes;
    }

    return ret;
}

/**
 * @brief Write contiguous blocks through the buffer cache (write-back).
 *
 * Function behavior:
 * - Stores each full block in the cache and marks it dirty; no device write here.
 * - Dirty contents reach the device on eviction or via `mofs_bcache_flush()`.
 * - Falls back to raw block I/O when the cache is not initialized.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] buf Source buffer of contiguous blocks.
 * @param[in] req_blk_num Number of blocks requested.
 * @param[in] start_blk_num Absolute starting block number.
 * @param[out] written_blk_num Number of full blocks accepted into the cache.
 * @param[out] fraction Always 0 for the cached path (kept for API parity).
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return Non-zero errno value propagated from a dirty-slot eviction flush.
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

    *fraction        = 0U;
    *written_blk_num = 0U;

    for (unsigned int i = 0U; i < req_blk_num; i++) {
        unsigned int blk = start_blk_num + i;
        int          idx = bcache_find(blk);

        if (idx < 0) {
            /* Allocate a slot for a not-yet-cached block. */
            ret = bcache_get_victim(&idx);
            if (ret != 0) {
                break;
            }
            bcache_pool[idx].blk_num = blk;
            bcache_pool[idx].valid   = MOFS_TRUE;
        }

        mofs_memcpy(bcache_pool[idx].data, buf, blk_bytes);
        bcache_pool[idx].dirty = MOFS_TRUE;
        bcache_touch(idx);

        *written_blk_num = *written_blk_num + 1U;
        buf              = (const char *)buf + blk_bytes;
    }

    return ret;
}

/**
 * @brief Flush all dirty cache entries to the device and sync.
 *
 * Function behavior:
 * - Writes every dirty slot back via raw block I/O.
 * - Issues a device sync so the data is durable after flush.
 *
 * @return 0 on success (also when the cache is not initialized).
 * @return Non-zero errno value propagated from raw block I/O or device sync.
 */
int mofs_bcache_flush(void)
{
    int ret = 0;

    if (bcache_ready != MOFS_TRUE) {
        return 0;
    }

    for (int i = 0; i < (int)MOFS_BUFFER_CACHE_NUM; i++) {
        if ((bcache_pool[i].valid == MOFS_TRUE) && (bcache_pool[i].dirty == MOFS_TRUE)) {
            ret = bcache_flush_entry(i);
            if (ret != 0) {
                break;
            }
        }
    }

    /* Make flushed blocks durable on the underlying device. */
    if (ret == 0) {
        if (dev_fsync(ctx.dev_fd) != 0) {
            ret = get_errno();
        }
    }

    return ret;
}

/**
 * @brief Drop a cached block without writing it back.
 *
 * Function behavior:
 * - Discards the slot holding `blk_num` so a later access reloads from device.
 * - Used when an absolute block is freed/repurposed (e.g. data block reuse),
 *   where stale dirty contents must not be flushed onto the new owner.
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

    idx = bcache_find(blk_num);
    if (idx >= 0) {
        bcache_pool[idx].valid = MOFS_FALSE;
        bcache_pool[idx].dirty = MOFS_FALSE;
    }

    return 0;
}
