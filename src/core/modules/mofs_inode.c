#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_time.h>
#include <mofs_type.h>

/**
 * @brief Update selected inode timestamp fields to the current time.
 *
 * Function behavior:
 * - Obtains current time via `mofs_now()`.
 * - Updates fields selected by `mask` (`MOFS_INODE_TIME_*`).
 *
 * @param[in,out] inode Inode structure to update in place.
 * @param[in] mask Bitmask of `MOFS_INODE_TIME_*` values.
 * @return 0 on success.
 * @return MOFS_EINVAL if `inode` is NULL or `mask` is zero.
 * @return Non-zero errno value propagated from `mofs_now()`.
 */
int mofs_inode_stamp_now(mofs_inode_t *inode, unsigned int mask)
{
    mofs_time_sec_t now = MOFS_TIME_INVALID;
    int             ret = 0;

    if ((inode == NULL) || (mask == 0U)) {
        return MOFS_EINVAL;
    }

    ret = mofs_now(&now);
    if (ret != 0) {
        return ret;
    }

    if ((mask & MOFS_INODE_TIME_ATIME) != 0U) {
        inode->i_atime = now;
    }
    if ((mask & MOFS_INODE_TIME_MTIME) != 0U) {
        inode->i_mtime = now;
    }
    if ((mask & MOFS_INODE_TIME_CTIME) != 0U) {
        inode->i_ctime = now;
    }

    return 0;
}

/**
 * @brief Set or clear one bit in the inode bitmap.
 *
 * Function behavior:
 * - Reads the inode-bitmap block containing `inode_idx`.
 * - Updates only the target bit to used/free state.
 * - Writes the bitmap block back to disk.
 *
 * @param[in] inode_idx Inode index in inode table.
 * @param[in] set_used true to mark used, false to mark free.
 * @return 0 on success.
 * @return MOFS_EINVAL if `inode_idx` is out of allocatable range.
 * @return MOFS_EIO if short read/write is detected on bitmap I/O.
 * @return Non-zero errno value propagated from block I/O.
 */
static int set_inode_bitmap_bit(unsigned int inode_idx, bool set_used)
{
    int               ret               = 0;
    uint32_t const    bb                = ctx.sp_blk.blk_size;
    size_t const      blk_sz            = (size_t)bb;
    unsigned int      bits_per_bitmap   = bb * 8U;
    unsigned int      target_blk        = inode_idx / bits_per_bitmap;
    unsigned int      bit_in_blk        = inode_idx % bits_per_bitmap;
    unsigned int      target_byte       = bit_in_blk / 8U;
    unsigned int      target_bit        = bit_in_blk % 8U;
    unsigned char    *bitmap_buf        = NULL;
    unsigned int      read_blk_num      = 0U;
    unsigned int      written_blk_num   = 0U;
    size_t            fraction          = 0U;

    if ((inode_idx < 3U) || (inode_idx >= ctx.sp_blk.inode_num)) {
        return MOFS_EINVAL;
    }

    bitmap_buf = (unsigned char *)mofs_malloc(blk_sz);
    if (bitmap_buf == NULL) {
        return get_errno();
    }

    ret = read_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.inode_bitmap_start + target_blk, &read_blk_num,
                                 &fraction);
    if (ret != 0) {
        goto out;
    }
    if ((read_blk_num != 1U) || (fraction != 0U)) {
        ret = MOFS_EIO;
        goto out;
    }

    if (set_used) {
        bitmap_buf[target_byte] |= (uint8_t)(1U << target_bit);
    } else {
        bitmap_buf[target_byte] &= (uint8_t)(~(1U << target_bit));
    }

    ret = write_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.inode_bitmap_start + target_blk, &written_blk_num,
                                  &fraction);
    if (ret != 0) {
        goto out;
    }
    if ((written_blk_num != 1U) || (fraction != 0U)) {
        ret = MOFS_EIO;
    }

out:
    mofs_free(bitmap_buf);
    return ret;
}

/**
 * @brief Apply used/free update to multiple inode bitmap entries.
 *
 * Function behavior:
 * - Iterates all inode indices in `inode_idx_list`.
 * - Applies best-effort bitmap update per element.
 * - Ignores individual update errors (for rollback paths).
 *
 * @param[in] inode_idx_list Array of inode indices.
 * @param[in] inode_num Number of elements in `inode_idx_list`.
 * @param[in] set_used true to mark used, false to mark free.
 */
static void update_inode_bitmap_bits(const unsigned int *inode_idx_list, unsigned int inode_num, bool set_used)
{
    if (inode_idx_list == NULL) {
        return;
    }

    for (unsigned int i = 0U; i < inode_num; i++) {
        (void)set_inode_bitmap_bit(inode_idx_list[i], set_used);
    }
}

/**
 * @brief Find free inode indices from on-disk inode bitmap.
 *
 * Function behavior:
 * - Scans inode-bitmap area block by block.
 * - Collects zero-bit entries as free inode indices.
 * - Skips reserved inode indices 0, 1 and 2.
 * - Stops when requested count is satisfied or bitmap ends.
 *
 * @param[out] allocated_inode_idx Output array for found free inode indices.
 * @param[in] required_alloc_num Number of free inode indices to collect.
 * @param[out] allocated_num Number of actually collected indices.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOSPC if free inode entries are insufficient.
 * @return MOFS_EIO if short inode-bitmap block read is detected.
 * @return Non-zero errno value propagated from block I/O.
 */
static int find_free_inode_indices(unsigned int *allocated_inode_idx, unsigned int required_alloc_num,
                                   unsigned int *allocated_num)
{
    int          ret            = 0;
    unsigned int bitmap_blk_num = 0U;

    if ((allocated_inode_idx == NULL) || (allocated_num == NULL)) {
        return MOFS_EINVAL;
    }

    *allocated_num = 0U;
    if (required_alloc_num == 0U) {
        return 0;
    }

    bitmap_blk_num = ctx.sp_blk.data_bitmap_start - ctx.sp_blk.inode_bitmap_start;

    uint32_t const bb                = ctx.sp_blk.blk_size;
    unsigned int   bits_per_bitmap   = bb * 8U;
    size_t const   blk_sz            = (size_t)bb;
    unsigned char *bitmap_buf        = NULL;

    bitmap_buf = (unsigned char *)mofs_malloc(blk_sz);
    if (bitmap_buf == NULL) {
        return get_errno();
    }

    for (unsigned int blk_idx = 0U; (blk_idx < bitmap_blk_num) && (*allocated_num < required_alloc_num); blk_idx++) {
        unsigned int read_blk_num = 0U;
        size_t       fraction      = 0U;

        ret = read_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.inode_bitmap_start + blk_idx, &read_blk_num,
                                     &fraction);
        if (ret != 0) {
            break;
        }
        if ((read_blk_num != 1U) || (fraction != 0U)) {
            ret = MOFS_EIO;
            break;
        }

        for (unsigned int byte_idx = 0U; (byte_idx < bb) && (*allocated_num < required_alloc_num); byte_idx++) {
            for (unsigned int bit_idx = 0U; (bit_idx < 8U) && (*allocated_num < required_alloc_num); bit_idx++) {
                unsigned int inode_idx = blk_idx * bits_per_bitmap + byte_idx * 8U + bit_idx;
                if (inode_idx >= ctx.sp_blk.inode_num) {
                    break;
                }
                if (inode_idx < 3U) {
                    continue;
                }
                if ((bitmap_buf[byte_idx] & (1U << bit_idx)) == 0U) {
                    allocated_inode_idx[*allocated_num] = inode_idx;
                    *allocated_num                      = *allocated_num + 1U;
                }
            }
        }
    }

    if ((ret == 0) && (*allocated_num < required_alloc_num)) {
        ret = MOFS_ENOSPC;
    }

    mofs_free(bitmap_buf);
    return ret;
}

/**
 * @brief Allocate one free inode entry from inode bitmap.
 *
 * Function behavior:
 * - Finds one free inode index from on-disk inode bitmap.
 * - Marks the selected inode bit as used.
 * - Returns allocated inode number through `inode_num`.
 *
 * @param[out] inode_num Destination pointer for allocated inode number.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOSPC if no allocatable inode is available.
 * @return Non-zero errno value propagated from bitmap I/O helpers.
 */
int allocate_inode(int *inode_num)
{
    int          ret = 0;
    unsigned int allocated_inode_idx[1];
    unsigned int allocated_num = 0U;

    if (inode_num == NULL) {
        return MOFS_EINVAL;
    }

    ret = find_free_inode_indices(allocated_inode_idx, 1U, &allocated_num);
    if (ret != 0) {
        return ret;
    }

    ret = set_inode_bitmap_bit(allocated_inode_idx[0], true);
    if (ret != 0) {
        update_inode_bitmap_bits(allocated_inode_idx, allocated_num, false);
        return ret;
    }

    *inode_num = (int)allocated_inode_idx[0];
    return 0;
}

/**
 * @brief Free one allocated inode entry.
 *
 * Function behavior:
 * - Clears the target inode bit in on-disk inode bitmap.
 * - Writes a zero-initialized inode entry to inode table.
 * - Restores inode bitmap bit when inode-table writeback fails.
 *
 * @param[in] inode_num Inode number to free.
 * @return 0 on success.
 * @return MOFS_EINVAL if inode number is invalid or reserved.
 * @return Non-zero errno value propagated from inode/bitmap I/O helpers.
 */
int free_inode(int inode_num)
{
    int          ret = 0;
    mofs_inode_t inode_buf;
    unsigned int inode_idx_list[1];

    if ((inode_num < 3) || (ctx.sp_blk.inode_num <= inode_num)) {
        return MOFS_EINVAL;
    }

    inode_idx_list[0] = (unsigned int)inode_num;

    ret = set_inode_bitmap_bit((unsigned int)inode_num, false);
    if (ret != 0) {
        return ret;
    }

    mofs_memset(&inode_buf, 0, sizeof(inode_buf));
    ret = mofs_write_inode(inode_num, &inode_buf);
    if (ret != 0) {
        update_inode_bitmap_bits(inode_idx_list, 1U, true);
        return ret;
    }

    return 0;
}

/**
 * @brief Read an inode entry from the on-disk inode table.
 *
 * Function behavior:
 * - Computes the inode-table block offset and in-block inode offset.
 * - Reads one inode-table block from disk.
 * - Copies the target inode entry into the caller-provided structure.
 *
 * @param[in] inode_num Inode number to read (zero-based index in table).
 * @param[out] inode Destination pointer to receive inode data.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return Non-zero errno value from `get_errno()` on seek/read/allocation
 *         failures.
 */
int mofs_read_inode(int inode_num, mofs_inode_t *inode)
{
    int          ret          = 0;
    off_t        blk_offset   = 0;
    off_t        inode_offset = 0;
    unsigned int read_blk_num = 0;
    size_t       fraction     = 0;
    void        *buf          = NULL;
    void        *inode_ptr    = NULL;
    uint32_t     bb           = ctx.sp_blk.blk_size;

    if ((inode_num < 0) || (ctx.sp_blk.inode_num <= inode_num) || (inode == NULL)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        buf = mofs_malloc((size_t)bb);
        if (buf == NULL) {
            ret = get_errno();
        }
    }

    blk_offset = ctx.sp_blk.inode_table_start;
    blk_offset += (inode_num * (int)sizeof(mofs_inode_t)) / (int)bb;
    inode_offset = (inode_num * (int)sizeof(mofs_inode_t)) % (int)bb;

    if (ret == 0) {
        ret = read_continuous_blocks(ctx.dev_fd, buf, 1, blk_offset, &read_blk_num, &fraction);
        if (ret != 0) {
            /* Do nothing */
        } else if ((read_blk_num != 1) || (fraction != 0)) {
            ret = MOFS_EIO;
        } else {
            inode_ptr = (char *)buf + inode_offset;
            mofs_memcpy(inode, inode_ptr, sizeof(mofs_inode_t));
        }
    }

    if (buf != NULL) {
        mofs_free(buf);
    }

    return ret;
}

/**
 * @brief Write an inode entry to the on-disk inode table.
 *
 * Function behavior:
 * - Validates inode number range and input pointers.
 * - Computes the inode-table block offset and in-block inode offset.
 * - Reads one inode-table block, updates only the target inode entry in the
 *   block buffer, then writes the block back to disk.
 *
 * @param[in] inode_num Inode number to write (zero-based index in table).
 * @param[in] inode Source inode data to be written.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EIO if a short block read/write is detected.
 * @return Non-zero errno value from lower-level I/O/allocation failures.
 */
int mofs_write_inode(int inode_num, const mofs_inode_t *inode)
{
    int          ret             = 0;
    off_t        blk_offset      = 0;
    off_t        inode_offset    = 0;
    unsigned int written_blk_num = 0;
    unsigned int read_blk_num    = 0;
    size_t       fraction        = 0;
    void        *blk_buf         = NULL;
    uint32_t     bb              = ctx.sp_blk.blk_size;

    if ((inode_num < 0) || (ctx.sp_blk.inode_num <= inode_num) || (inode == NULL)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        blk_buf = mofs_malloc((size_t)bb);
        if (blk_buf == NULL) {
            ret = get_errno();
        }
    }

    /* Copy inode to block buffer */
    if (ret == 0) {
        blk_offset = ctx.sp_blk.inode_table_start;
        blk_offset += (inode_num * (int)sizeof(mofs_inode_t)) / (int)bb;
        inode_offset = (inode_num * (int)sizeof(mofs_inode_t)) % (int)bb;

        ret = read_continuous_blocks(ctx.dev_fd, blk_buf, 1, blk_offset, &read_blk_num, &fraction);
        if (ret != 0) {
            /* Do nothing */
        } else if ((read_blk_num != 1) || (fraction != 0)) {
            ret = MOFS_EIO;
        } else {
            mofs_memcpy((char *)blk_buf + inode_offset, inode, sizeof(mofs_inode_t));
        }
    }

    if (ret == 0) {
        ret = write_continuous_blocks(ctx.dev_fd, blk_buf, 1, blk_offset, &written_blk_num, &fraction);
        if (ret != 0) {
            /* Do nothing */
        } else if ((written_blk_num != 1) || (fraction != 0)) {
            ret = MOFS_EIO;
        }
    }

    if (blk_buf != NULL) {
        mofs_free(blk_buf);
    }

    return ret;
}
