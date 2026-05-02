
#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_type.h>

/**
 * @brief Set or clear one bit in the data bitmap.
 *
 * Function behavior:
 * - Reads the bitmap block containing `data_idx`.
 * - Updates only the target bit to used/free state.
 * - Writes the bitmap block back to disk.
 *
 * @param[in] data_idx Data-region relative block index.
 * @param[in] set_used true to mark used, false to mark free.
 * @return 0 on success.
 * @return MOFS_EINVAL if `data_idx` is out of range.
 * @return MOFS_EIO if short read/write is detected on bitmap I/O.
 * @return Non-zero errno value propagated from block I/O.
 */
static int set_data_bitmap_bit(unsigned int data_idx, bool set_used)
{
    int           ret             = 0;
    unsigned int  target_blk      = data_idx / (MOFS_BLK_SIZE * 8U);
    unsigned int  bit_in_blk      = data_idx % (MOFS_BLK_SIZE * 8U);
    unsigned int  target_byte     = bit_in_blk / 8U;
    unsigned int  target_bit      = bit_in_blk % 8U;
    unsigned char bitmap_buf[MOFS_BLK_SIZE];
    unsigned int  read_blk_num    = 0U;
    unsigned int  written_blk_num = 0U;
    size_t        fraction        = 0U;

    if (data_idx >= ctx.sp_blk.data_blk_num) {
        return MOFS_EINVAL;
    }

    /* Load the bitmap block that owns target bit. */
    ret = read_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.data_bitmap_start + target_blk, &read_blk_num,
                                 &fraction);
    if (ret != 0) {
        return ret;
    }
    if ((read_blk_num != 1U) || (fraction != 0U)) {
        return MOFS_EIO;
    }

    /* Toggle exactly one bit for the target data block. */
    if (set_used) {
        bitmap_buf[target_byte] |= (uint8_t)(1U << target_bit);
    } else {
        bitmap_buf[target_byte] &= (uint8_t)(~(1U << target_bit));
    }

    /* Persist bitmap block update. */
    ret = write_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.data_bitmap_start + target_blk, &written_blk_num,
                                  &fraction);
    if (ret != 0) {
        return ret;
    }
    if ((written_blk_num != 1U) || (fraction != 0U)) {
        return MOFS_EIO;
    }

    return 0;
}

/**
 * @brief Apply used/free update to multiple data bitmap entries.
 *
 * Function behavior:
 * - Iterates all data indices in `data_idx_list`.
 * - Applies best-effort bitmap update per element.
 * - Ignores individual update errors (for rollback paths).
 *
 * @param[in] data_idx_list Array of data-region relative block indices.
 * @param[in] data_num Number of elements in `data_idx_list`.
 * @param[in] set_used true to mark used, false to mark free.
 */
static void update_data_bitmap_bits(const unsigned int *data_idx_list, unsigned int data_num, bool set_used)
{
    if (data_idx_list == NULL) {
        return;
    }

    for (unsigned int i = 0U; i < data_num; i++) {
        (void)set_data_bitmap_bit(data_idx_list[i], set_used);
    }
}

/**
 * @brief Find free data-block indices from on-disk bitmap.
 *
 * Function behavior:
 * - Scans data-bitmap area block by block.
 * - Collects zero-bit entries as free data blocks.
 * - Stops when requested count is satisfied or bitmap ends.
 *
 * @param[out] allocated_data_idx Output array for found free indices.
 * @param[in] required_alloc_num Number of free indices to collect.
 * @param[out] allocated_num Number of actually collected indices.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOSPC if free blocks are insufficient.
 * @return MOFS_EIO if short bitmap block read is detected.
 * @return Non-zero errno value propagated from block I/O.
 */
static int find_free_data_block_indices(unsigned int *allocated_data_idx, unsigned int required_alloc_num,
                                        unsigned int *allocated_num)
{
    int          ret            = 0;
    unsigned int bitmap_blk_num = 0U;

    if ((allocated_data_idx == NULL) || (allocated_num == NULL)) {
        return MOFS_EINVAL;
    }

    *allocated_num = 0U;
    if (required_alloc_num == 0U) {
        return 0;
    }

    bitmap_blk_num = ctx.sp_blk.inode_table_start - ctx.sp_blk.data_bitmap_start;

    /* Scan each bitmap block until enough free blocks are found. */
    for (unsigned int blk_idx = 0U; (blk_idx < bitmap_blk_num) && (*allocated_num < required_alloc_num); blk_idx++) {
        unsigned char bitmap_buf[MOFS_BLK_SIZE];
        unsigned int  read_blk_num = 0U;
        size_t        fraction     = 0U;

        ret = read_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.data_bitmap_start + blk_idx, &read_blk_num,
                                     &fraction);
        if (ret != 0) {
            break;
        }
        if ((read_blk_num != 1U) || (fraction != 0U)) {
            ret = MOFS_EIO;
            break;
        }

        /* Enumerate bits in the bitmap block and capture free entries. */
        for (unsigned int byte_idx = 0U; (byte_idx < MOFS_BLK_SIZE) && (*allocated_num < required_alloc_num); byte_idx++) {
            for (unsigned int bit_idx = 0U; (bit_idx < 8U) && (*allocated_num < required_alloc_num); bit_idx++) {
                unsigned int data_idx = blk_idx * (MOFS_BLK_SIZE * 8U) + byte_idx * 8U + bit_idx;
                if (data_idx >= ctx.sp_blk.data_blk_num) {
                    break;
                }
                if ((bitmap_buf[byte_idx] & (1U << bit_idx)) == 0U) {
                    allocated_data_idx[*allocated_num] = data_idx;
                    *allocated_num                     = *allocated_num + 1U;
                }
            }
        }
    }

    if ((ret == 0) && (*allocated_num < required_alloc_num)) {
        ret = MOFS_ENOSPC;
    }

    return ret;
}

/**
 * @brief Allocate data blocks for a file by appending at EOF-side slots.
 *
 * Function behavior:
 * - Reads inode and computes currently used file data-block count from size.
 * - Finds free data blocks in bitmap and marks them as used.
 * - Appends allocated absolute block numbers to inode data-block slots.
 * - Rolls back bitmap updates if inode write fails.
 *
 * @param[in] inode_num Target inode number.
 * @param[in] req_blk_num Number of blocks to newly allocate.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOSPC if file block limit or free space is insufficient.
 * @return Non-zero errno value propagated from inode/block operations.
 */
int allocate_data_block(int inode_num, unsigned int req_blk_num)
{
    int          ret = 0;
    mofs_inode_t inode_buf;
    unsigned int used_blk_num = 0U;
    unsigned int allocated_data_idx[MOFS_DATA_BLK_PER_FILE];
    unsigned int allocated_num = 0U;

    if (inode_num < 0) {
        return MOFS_EINVAL;
    }
    if (req_blk_num == 0U) {
        return 0;
    }

    ret = mofs_read_inode(inode_num, &inode_buf);
    if (ret != 0) {
        return ret;
    }

    used_blk_num = (inode_buf.i_size + MOFS_BLK_SIZE - 1U) / MOFS_BLK_SIZE;
    if ((used_blk_num + req_blk_num) > MOFS_DATA_BLK_PER_FILE) {
        return MOFS_ENOSPC;
    }

    /* Find free block indices from bitmap area. */
    ret = find_free_data_block_indices(allocated_data_idx, req_blk_num, &allocated_num);

    /* Mark selected bitmap bits as used. */
    if (ret == 0) {
        for (unsigned int i = 0U; i < allocated_num; i++) {
            ret = set_data_bitmap_bit(allocated_data_idx[i], true);
            if (ret != 0) {
                break;
            }
        }
    }

    /* Roll back bitmap reservation on failure. */
    if (ret != 0) {
        update_data_bitmap_bits(allocated_data_idx, allocated_num, false);
        return ret;
    }

    /* Append allocated blocks at EOF-side slots. */
    for (unsigned int i = 0U; i < req_blk_num; i++) {
        inode_buf.i_data_blk[used_blk_num + i] = ctx.sp_blk.data_region_start + allocated_data_idx[i];
    }

    ret = mofs_write_inode(inode_num, &inode_buf);
    if (ret != 0) {
        update_data_bitmap_bits(allocated_data_idx, req_blk_num, false);
    }

    return ret;
}

/**
 * @brief Free file data blocks and compact inode slot mapping.
 *
 * Function behavior:
 * - Converts target inode slots to data-region indices and frees bitmap bits.
 * - Compacts `i_data_blk[]` by left-shifting remaining entries.
 * - Clears trailing inode slots after compaction.
 * - Restores freed bitmap bits when inode writeback fails.
 *
 * @param[in] inode_num Target inode number.
 * @param[in] start_blk_num File-relative start slot to free.
 * @param[in] req_blk_num Number of slots to free from `start_blk_num`.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments or range are invalid.
 * @return MOFS_EIO if inode slot has invalid block number.
 * @return Non-zero errno value propagated from inode/block operations.
 */
int free_data_block(int inode_num, unsigned int start_blk_num, unsigned int req_blk_num)
{
    int          ret = 0;
    mofs_inode_t inode_buf;
    unsigned int used_blk_num = 0U;
    unsigned int free_blk_num = 0U;
    unsigned int freed_num    = 0U;
    unsigned int freed_data_idx[MOFS_DATA_BLK_PER_FILE];

    if ((inode_num < 0) || (start_blk_num >= MOFS_DATA_BLK_PER_FILE)) {
        return MOFS_EINVAL;
    }
    if (req_blk_num == 0U) {
        return 0;
    }

    ret = mofs_read_inode(inode_num, &inode_buf);
    if (ret != 0) {
        return ret;
    }

    used_blk_num = (inode_buf.i_size + MOFS_BLK_SIZE - 1U) / MOFS_BLK_SIZE;
    if (start_blk_num >= used_blk_num) {
        return MOFS_EINVAL;
    }

    free_blk_num = req_blk_num;
    if ((start_blk_num + free_blk_num) > used_blk_num) {
        free_blk_num = used_blk_num - start_blk_num;
    }

    /* Free each bitmap bit referenced by target inode slots. */
    for (unsigned int i = 0U; i < free_blk_num; i++) {
        unsigned int abs_blk = inode_buf.i_data_blk[start_blk_num + i];
        if (abs_blk < ctx.sp_blk.data_region_start) {
            ret = MOFS_EIO;
            break;
        }
        freed_data_idx[i] = abs_blk - ctx.sp_blk.data_region_start;
        if (freed_data_idx[i] >= ctx.sp_blk.data_blk_num) {
            ret = MOFS_EIO;
            break;
        }

        ret = set_data_bitmap_bit(freed_data_idx[i], false);
        if (ret != 0) {
            break;
        }
        freed_num++;
    }

    if (ret != 0) {
        /* Restore bits changed before failure. */
        for (unsigned int i = 0U; i < freed_num; i++) {
            (void)set_data_bitmap_bit(freed_data_idx[i], true);
        }
        return ret;
    }

    /* Compact inode data-block slots to keep mapping contiguous. */
    for (unsigned int i = start_blk_num; (i + free_blk_num) < used_blk_num; i++) {
        inode_buf.i_data_blk[i] = inode_buf.i_data_blk[i + free_blk_num];
    }
    /* Clear tail slots that became unused by compaction. */
    for (unsigned int i = used_blk_num - free_blk_num; i < used_blk_num; i++) {
        inode_buf.i_data_blk[i] = 0U;
    }

    ret = mofs_write_inode(inode_num, &inode_buf);
    if (ret != 0) {
        update_data_bitmap_bits(freed_data_idx, free_blk_num, true);
    }

    return ret;
}

/**
 * @brief Read exactly one filesystem block from the current device offset.
 *
 * Function behavior:
 * - Calls `dev_read()` for `MOFS_BLK_SIZE` bytes.
 * - Converts a low-level read error (`-1`) into `0` and reports the error
 *   code through `err`.
 *
 * @param[in] fd Device file descriptor.
 * @param[out] buf Destination buffer for one block.
 * @param[out] err Error code storage. Set to 0 on no low-level read error.
 * @return Number of bytes read (typically `MOFS_BLK_SIZE` or a short read).
 * @return 0 when `dev_read()` fails with `-1` (details are stored in `*err`).
 */
static int read_one_block(int fd, void *buf, int *err)
{
    int ret = dev_read(fd, buf, MOFS_BLK_SIZE);

    if (ret == -1) {
        *err = get_errno();
        ret  = 0;
    } else {
        *err = 0;
    }

    return ret;
}

/**
 * @brief Read multiple contiguous filesystem blocks from the current offset.
 *
 * Function behavior:
 * - Validates arguments and block alignment of the current file position.
 * - Repeatedly reads one block until `blk_num` blocks are read, a short read
 *   occurs, or an error is detected.
 * - Updates the number of full blocks read and the short-read remainder.
 *
 * @param[in] fd Device file descriptor.
 * @param[out] buf Destination buffer for contiguous blocks.
 * @param[in] req_blk_num Number of blocks requested.
 * @param[in] start_blk_num Starting block number.
 * @param[out] read_blk_num Number of full blocks successfully read.
 * @param[out] fraction Number of bytes for a short read in the last attempt.
 * @return 0 on success (including short-read case; see `fraction`).
 * @return MOFS_EINVAL if arguments are invalid or offset is not block-aligned.
 * @return Non-zero errno value from `get_errno()` on read-related failures.
 */
int read_continuous_blocks(int fd, void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                           unsigned int *read_blk_num, size_t *fraction)
{
    int err = 0;
    int ret = 0;

    if ((fd < 0) || (buf == NULL) || (read_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* Seek to start block */
    if (ret == 0) {
        off_t offset = dev_lseek(fd, start_blk_num * MOFS_BLK_SIZE, MOFS_SEEK_SET);
        if (offset < 0) {
            ret = get_errno();
        } else if ((offset % MOFS_BLK_SIZE) != 0) {
            ret = MOFS_EINVAL;
        }
    }

    if (ret == 0) {
        *fraction     = 0U;
        *read_blk_num = 0U;

        for (int i = 0; i < req_blk_num; i++) {
            ret = read_one_block(fd, buf, &err);
            if (ret == 0) {
                ret = err;
                break;
            } else if (ret != MOFS_BLK_SIZE) {
                *fraction = ret;
                ret       = 0;
                break;
            } else {
                ret = 0;
            }
            *read_blk_num = *read_blk_num + 1;
            buf           = (char *)buf + MOFS_BLK_SIZE;
        }
    }

    return ret;
}

/**
 * @brief Write exactly one filesystem block at the current device offset.
 *
 * Function behavior:
 * - Calls `dev_write()` for `MOFS_BLK_SIZE` bytes.
 * - Converts a low-level write error (`-1`) into `0` and reports the error
 *   code through `err`.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] buf Source buffer containing one block to write.
 * @param[out] err Error code storage. Set to 0 on no low-level write error.
 * @return Number of bytes written (typically `MOFS_BLK_SIZE` or a short write).
 * @return 0 when `dev_write()` fails with `-1` (details are stored in `*err`).
 */
static int write_one_block(int fd, const void *buf, int *err)
{
    int ret = dev_write(fd, buf, MOFS_BLK_SIZE);

    if (ret == -1) {
        *err = get_errno();
        ret  = 0;
    } else {
        *err = 0;
    }

    return ret;
}

/**
 * @brief Write multiple contiguous filesystem blocks.
 *
 * Function behavior:
 * - Validates arguments and block alignment of the current file position.
 * - Repeatedly writes one block until `blk_num` blocks are written, a short
 *   write occurs, or an error is detected.
 * - Updates the number of full blocks written and the short-write remainder.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] buf Source buffer containing contiguous blocks.
 * @param[in] req_blk_num Number of blocks requested to write.
 * @param[in] start_blk_num Starting block number.
 * @param[out] written_blk_num Number of full blocks successfully written.
 * @param[out] fraction Number of bytes for a short write in the last attempt.
 * @return 0 on success (including short-write case; see `fraction`).
 * @return MOFS_EINVAL if arguments are invalid or offset is not block-aligned.
 * @return Non-zero errno value from `get_errno()` on write-related failures.
 */
int write_continuous_blocks(int fd, const void *buf, unsigned int req_blk_num, unsigned int start_blk_num,
                            unsigned int *written_blk_num, size_t *fraction)
{
    int err = 0;
    int ret = 0;

    if ((fd < 0) || (buf == NULL) || (written_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* Align check */
    if (ret == 0) {
        off_t offset = dev_lseek(fd, start_blk_num * MOFS_BLK_SIZE, MOFS_SEEK_SET);
        if (offset < 0) {
            ret = get_errno();
        } else if ((offset % MOFS_BLK_SIZE) != 0) {
            ret = MOFS_EINVAL;
        }
    }

    if (ret == 0) {
        *fraction        = 0U;
        *written_blk_num = 0U;

        if (ret == 0) {
            for (int i = 0; i < req_blk_num; i++) {
                ret = write_one_block(fd, buf, &err);
                if (ret == 0) {
                    ret = err;
                    break;
                } else if (ret != MOFS_BLK_SIZE) {
                    *fraction = ret;
                    ret       = 0;
                    break;
                } else {
                    ret = 0;
                }
                *written_blk_num = *written_blk_num + 1;
                buf              = (char *)buf + MOFS_BLK_SIZE;
            }
        }
    }

    return ret;
}
