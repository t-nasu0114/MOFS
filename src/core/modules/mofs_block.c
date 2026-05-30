
#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_type.h>

/**
 * @brief Return the mounted volume logical block size in bytes.
 *
 * @return Logical block size from `ctx.sp_blk.blk_size`.
 */
static size_t mofs_io_blk_sz(void)
{
    return (size_t)ctx.sp_blk.blk_size;
}

static int  set_data_bitmap_bit(unsigned int data_idx, bool set_used);
static void update_data_bitmap_bits(const unsigned int *data_idx_list, unsigned int data_num, bool set_used);
static int  find_free_data_block_indices(unsigned int *allocated_data_idx, unsigned int required_alloc_num,
                                         unsigned int *allocated_num);

/**
 * @brief Maximum file-data pointers storable in one on-disk list node.
 *
 * @return Pointer capacity for the current `ctx.sp_blk.blk_size`.
 */
static unsigned int list_ptrs_cap(void)
{
    return mofs_list_ptrs_per_node(ctx.sp_blk.blk_size);
}

/**
 * @brief Pointer to the data-block array inside a list-node buffer.
 *
 * @param[in] blk_buf Buffer of one logical block holding a list node.
 * @return Address of the first `uint32_t` file data pointer.
 */
static uint32_t *list_ptr_area(unsigned char *blk_buf)
{
    return (uint32_t *)(blk_buf + sizeof(mofs_data_list_hdr_t));
}

/**
 * @brief Convert an absolute data-region block number to a bitmap index.
 *
 * Function behavior:
 * - Subtracts `data_region_start` from `abs_blk`.
 * - Rejects values outside the allocatable data region.
 *
 * @param[in] abs_blk Absolute block number on the device.
 * @param[out] data_idx_out Data-region relative index for the bitmap.
 * @return 0 on success.
 * @return MOFS_EIO if `abs_blk` is not in the data region.
 */
static int abs_to_data_idx(unsigned int abs_blk, unsigned int *data_idx_out)
{
    if (abs_blk < ctx.sp_blk.data_region_start) {
        return MOFS_EIO;
    }
    *data_idx_out = abs_blk - ctx.sp_blk.data_region_start;
    if (*data_idx_out >= ctx.sp_blk.data_blk_num) {
        return MOFS_EIO;
    }
    return 0;
}

/**
 * @brief Read one on-disk list node block into memory.
 *
 * @param[in] abs_blk Absolute block number of the list node.
 * @param[out] blk_buf Buffer of one logical block.
 * @return 0 on success.
 * @return MOFS_EIO if a full block was not read.
 * @return Non-zero errno value propagated from block I/O.
 */
static int read_list_node(unsigned int abs_blk, unsigned char *blk_buf)
{
    unsigned int read_blk_num = 0U;
    size_t       fraction     = 0U;
    int          ret;

    ret = read_continuous_blocks(ctx.dev_fd, blk_buf, 1U, abs_blk, &read_blk_num, &fraction);
    if (ret != 0) {
        return ret;
    }
    if ((read_blk_num != 1U) || (fraction != 0U)) {
        return MOFS_EIO;
    }
    return 0;
}

/**
 * @brief Write one in-memory list node buffer to disk.
 *
 * @param[in] abs_blk Absolute block number of the list node.
 * @param[in] blk_buf Buffer of one logical block.
 * @return 0 on success.
 * @return MOFS_EIO if a full block was not written.
 * @return Non-zero errno value propagated from block I/O.
 */
static int write_list_node(unsigned int abs_blk, unsigned char *blk_buf)
{
    unsigned int written_blk_num = 0U;
    size_t       fraction        = 0U;
    int          ret;

    ret = write_continuous_blocks(ctx.dev_fd, blk_buf, 1U, abs_blk, &written_blk_num, &fraction);
    if (ret != 0) {
        return ret;
    }
    if ((written_blk_num != 1U) || (fraction != 0U)) {
        return MOFS_EIO;
    }
    return 0;
}

/**
 * @brief Free every list-node block in a singly linked chain.
 *
 * Function behavior:
 * - Walks `next_abs` from `head_abs` until the chain ends.
 * - Clears the data bitmap bit for each list-node block (not file data blocks).
 *
 * @param[in] head_abs Absolute block of the first list node, or 0 for an empty chain.
 * @return 0 on success.
 * @return Non-zero errno value from list I/O or bitmap update.
 */
static int free_list_chain(unsigned int head_abs)
{
    unsigned int   node_abs = head_abs;
    unsigned char *blk_buf  = NULL;
    int            ret      = 0;

    if (head_abs == 0U) {
        return 0;
    }

    blk_buf = (unsigned char *)mofs_malloc(mofs_io_blk_sz());
    if (blk_buf == NULL) {
        return get_errno();
    }

    /* Visit each list node and release its bitmap entry. */
    while (node_abs != 0U) {
        mofs_data_list_hdr_t *hdr = NULL;
        unsigned int          data_idx;
        unsigned int          next_abs;

        ret = read_list_node(node_abs, blk_buf);
        if (ret != 0) {
            break;
        }

        hdr      = (mofs_data_list_hdr_t *)blk_buf;
        next_abs = hdr->next_abs;

        ret = abs_to_data_idx(node_abs, &data_idx);
        if (ret != 0) {
            break;
        }
        ret = set_data_bitmap_bit(data_idx, false);
        if (ret != 0) {
            break;
        }

        node_abs = next_abs;
    }

    mofs_free(blk_buf);
    return ret;
}

/**
 * @brief Export a file's on-disk data block list into a logical-order array.
 *
 * Function behavior:
 * - Walks list nodes starting at `inode->i_data_head`.
 * - Copies each file data pointer in logical block order into `abs_out`.
 * - Verifies the collected count matches `inode->i_nr_blocks`.
 *
 * @param[in] inode Inode containing `i_data_head` and `i_nr_blocks`.
 * @param[out] abs_out Output array with room for at least `inode->i_nr_blocks` entries.
 * @param[in] abs_cap Capacity of `abs_out` in elements.
 * @return 0 on success (including zero data blocks).
 * @return MOFS_EINVAL if arguments are inconsistent.
 * @return MOFS_EIO if on-disk list metadata is corrupt.
 */
static int export_data_block_list(const mofs_inode_t *inode, unsigned int *abs_out, unsigned int abs_cap)
{
    unsigned int   node_abs = inode->i_data_head;
    unsigned int   out_idx  = 0U;
    unsigned char *blk_buf  = NULL;
    int            ret      = 0;
    unsigned int   ptrs_cap = list_ptrs_cap();

    if (inode->i_nr_blocks == 0U) {
        return 0;
    }
    if ((inode->i_data_head == 0U) || (abs_out == NULL) || (abs_cap < inode->i_nr_blocks)) {
        return MOFS_EINVAL;
    }
    if (ptrs_cap == 0U) {
        return MOFS_EINVAL;
    }

    blk_buf = (unsigned char *)mofs_malloc(mofs_io_blk_sz());
    if (blk_buf == NULL) {
        return get_errno();
    }

    /* Export logical block 0 .. i_nr_blocks-1 from the linked list nodes. */
    while (node_abs != 0U) {
        mofs_data_list_hdr_t *hdr = NULL;
        unsigned int          i;

        ret = read_list_node(node_abs, blk_buf);
        if (ret != 0) {
            break;
        }

        hdr = (mofs_data_list_hdr_t *)blk_buf;
        if (hdr->nr_ptrs > ptrs_cap) {
            ret = MOFS_EIO;
            break;
        }

        for (i = 0U; i < hdr->nr_ptrs; i++) {
            if (out_idx >= inode->i_nr_blocks) {
                ret = MOFS_EIO;
                goto out;
            }
            abs_out[out_idx] = list_ptr_area(blk_buf)[i];
            out_idx++;
        }

        node_abs = hdr->next_abs;
    }

    if ((ret == 0) && (out_idx != inode->i_nr_blocks)) {
        ret = MOFS_EIO;
    }

out:
    mofs_free(blk_buf);
    return ret;
}

/**
 * @brief Compute how many list-node blocks are required for `nr_blocks` data pointers.
 *
 * @param[in] nr_blocks Number of file data blocks to map.
 * @return Required list-node count, or 0 when `nr_blocks` is 0.
 */
static unsigned int list_nodes_for_block_count(unsigned int nr_blocks)
{
    unsigned int ptrs_cap = list_ptrs_cap();

    if (nr_blocks == 0U) {
        return 0U;
    }
    if (ptrs_cap == 0U) {
        return 0U;
    }
    return (nr_blocks + ptrs_cap - 1U) / ptrs_cap;
}

/**
 * @brief Rebuild a file's on-disk data block list from a logical-order array.
 *
 * Function behavior:
 * - Records the previous `i_data_head` in `old_head_out` (caller frees after inode write).
 * - Allocates list-node blocks from the data bitmap and links them via `next_abs`.
 * - Writes file data pointers into each node without freeing file data blocks.
 * - Updates `inode->i_data_head` and `inode->i_nr_blocks` on success.
 *
 * @param[in,out] inode Inode to update in memory.
 * @param[in] abs_list Absolute block numbers in logical order (length `nr_blocks`).
 * @param[in] nr_blocks Number of file data blocks (0 clears the mapping).
 * @param[out] old_head_out Previous list head; unchanged when NULL.
 * @return 0 on success.
 * @return MOFS_ENOSPC if list-node blocks cannot be allocated.
 * @return Non-zero errno value from bitmap or block I/O.
 */
static int rebuild_data_block_list(mofs_inode_t *inode, const unsigned int *abs_list, unsigned int nr_blocks,
                             unsigned int *old_head_out)
{
    unsigned int   needed_nodes  = list_nodes_for_block_count(nr_blocks);
    unsigned int   ptrs_cap      = list_ptrs_cap();
    unsigned char *blk_buf       = NULL;
    unsigned int  *list_node_idx = NULL;
    unsigned int   list_node_num = 0U;
    unsigned int   abs_idx       = 0U;
    unsigned int   head_abs      = 0U;
    int            ret           = 0;

    if (old_head_out != NULL) {
        *old_head_out = inode->i_data_head;
    }

    if (nr_blocks == 0U) {
        inode->i_data_head = 0U;
        inode->i_nr_blocks = 0U;
        return 0;
    }

    if (needed_nodes == 0U) {
        return MOFS_EINVAL;
    }

    /* Reserve one bitmap entry per list-node block. */
    list_node_idx = (unsigned int *)mofs_malloc(needed_nodes * sizeof(unsigned int));
    if (list_node_idx == NULL) {
        return get_errno();
    }

    ret = find_free_data_block_indices(list_node_idx, needed_nodes, &list_node_num);
    if (ret != 0) {
        goto out;
    }

    for (unsigned int i = 0U; i < list_node_num; i++) {
        ret = set_data_bitmap_bit(list_node_idx[i], true);
        if (ret != 0) {
            update_data_bitmap_bits(list_node_idx, i, false);
            goto out;
        }
    }

    blk_buf = (unsigned char *)mofs_malloc(mofs_io_blk_sz());
    if (blk_buf == NULL) {
        ret = get_errno();
        update_data_bitmap_bits(list_node_idx, list_node_num, false);
        goto out;
    }

    /* Fill each list node with a slice of abs_list and chain to the next node. */
    for (unsigned int node_i = 0U; node_i < needed_nodes; node_i++) {
        mofs_data_list_hdr_t *hdr = (mofs_data_list_hdr_t *)blk_buf;
        unsigned int          nr_in_node;
        unsigned int          node_abs = ctx.sp_blk.data_region_start + list_node_idx[node_i];

        mofs_memset(blk_buf, 0, mofs_io_blk_sz());
        nr_in_node = nr_blocks - abs_idx;
        if (nr_in_node > ptrs_cap) {
            nr_in_node = ptrs_cap;
        }

        hdr->next_abs = 0U;
        if ((node_i + 1U) < needed_nodes) {
            hdr->next_abs = ctx.sp_blk.data_region_start + list_node_idx[node_i + 1U];
        }
        hdr->nr_ptrs = nr_in_node;

        for (unsigned int p = 0U; p < nr_in_node; p++) {
            list_ptr_area(blk_buf)[p] = abs_list[abs_idx + p];
        }
        abs_idx += nr_in_node;

        ret = write_list_node(node_abs, blk_buf);
        if (ret != 0) {
            update_data_bitmap_bits(list_node_idx, list_node_num, false);
            goto out;
        }

        if (node_i == 0U) {
            head_abs = node_abs;
        }
    }

    inode->i_data_head = head_abs;
    inode->i_nr_blocks = nr_blocks;

out:
    mofs_free(blk_buf);
    mofs_free(list_node_idx);
    return ret;
}

/**
 * @brief Map a file-relative block index to an absolute on-disk data block.
 *
 * Function behavior:
 * - Loads the inode and walks list nodes from `i_data_head`.
 * - Subtracts `nr_ptrs` per node until `file_blk_idx` falls inside the current node.
 * - Returns the selected pointer from that node's array.
 *
 * @param[in] inode_num Inode number of the file.
 * @param[in] file_blk_idx Logical data block index (0 .. i_nr_blocks-1).
 * @param[out] abs_blk_out Absolute block number of the data block.
 * @return 0 on success.
 * @return MOFS_EINVAL if `file_blk_idx` is out of range.
 * @return MOFS_EIO if the inode has no list or on-disk metadata is inconsistent.
 */
int resolve_file_data_block(int inode_num, unsigned int file_blk_idx, unsigned int *abs_blk_out)
{
    mofs_inode_t   inode;
    unsigned int   node_abs = 0U;
    unsigned int   remain   = 0U;
    unsigned char *blk_buf  = NULL;
    int            ret      = 0;

    if ((inode_num < 0) || (abs_blk_out == NULL)) {
        return MOFS_EINVAL;
    }

    ret = mofs_read_inode(inode_num, &inode);
    if (ret != 0) {
        return ret;
    }

    if (file_blk_idx >= inode.i_nr_blocks) {
        return MOFS_EINVAL;
    }
    if (inode.i_data_head == 0U) {
        return MOFS_EIO;
    }

    blk_buf = (unsigned char *)mofs_malloc(mofs_io_blk_sz());
    if (blk_buf == NULL) {
        return get_errno();
    }

    remain   = file_blk_idx;
    node_abs = inode.i_data_head;

    /* Advance node-by-node until the target index lies in the current node. */
    while (node_abs != 0U) {
        mofs_data_list_hdr_t *hdr = NULL;

        ret = read_list_node(node_abs, blk_buf);
        if (ret != 0) {
            break;
        }

        hdr = (mofs_data_list_hdr_t *)blk_buf;
        if (hdr->nr_ptrs > list_ptrs_cap()) {
            ret = MOFS_EIO;
            break;
        }

        if (remain < hdr->nr_ptrs) {
            *abs_blk_out = list_ptr_area(blk_buf)[remain];
            ret          = 0;
            break;
        }

        remain -= hdr->nr_ptrs;
        node_abs = hdr->next_abs;
    }

    /* Chain ended before the requested index was found. */
    if ((ret == 0) && (node_abs == 0U) && (remain >= 0U)) {
        ret = MOFS_EIO;
    }

    mofs_free(blk_buf);
    return ret;
}

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
    int            ret             = 0;
    size_t const   blk_sz          = mofs_io_blk_sz();
    unsigned int   bits_per_bitmap = (unsigned int)(blk_sz * 8U);
    unsigned int   target_blk      = data_idx / bits_per_bitmap;
    unsigned int   bit_in_blk      = data_idx % bits_per_bitmap;
    unsigned int   target_byte     = bit_in_blk / 8U;
    unsigned int   target_bit      = bit_in_blk % 8U;
    unsigned char *bitmap_buf      = NULL;
    unsigned int   read_blk_num    = 0U;
    unsigned int   written_blk_num = 0U;
    size_t         fraction        = 0U;

    if (data_idx >= ctx.sp_blk.data_blk_num) {
        return MOFS_EINVAL;
    }

    bitmap_buf = (unsigned char *)mofs_malloc(blk_sz);
    if (bitmap_buf == NULL) {
        return get_errno();
    }

    /* Load the bitmap block that owns target bit. */
    ret = read_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.data_bitmap_start + target_blk, &read_blk_num,
                                 &fraction);
    if (ret != 0) {
        goto out;
    }
    if ((read_blk_num != 1U) || (fraction != 0U)) {
        ret = MOFS_EIO;
        goto out;
    }

    /* Toggle exactly one bit for the target data block. */
    if (set_used) {
        bitmap_buf[target_byte] |= (uint8_t)(1U << target_bit);
    } else {
        bitmap_buf[target_byte] &= (uint8_t)(~(1U << target_bit));
    }

    /* Persist bitmap block update. */
    ret = write_continuous_blocks(ctx.dev_fd, bitmap_buf, 1U, ctx.sp_blk.data_bitmap_start + target_blk,
                                  &written_blk_num, &fraction);
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

    size_t const       blk_sz          = mofs_io_blk_sz();
    unsigned char     *bitmap_buf      = NULL;
    unsigned int const bits_per_bitmap = (unsigned int)(blk_sz * 8U);

    bitmap_buf = (unsigned char *)mofs_malloc(blk_sz);
    if (bitmap_buf == NULL) {
        return get_errno();
    }

    /* Scan each bitmap block until enough free blocks are found. */
    for (unsigned int blk_idx = 0U; (blk_idx < bitmap_blk_num) && (*allocated_num < required_alloc_num); blk_idx++) {
        unsigned int read_blk_num = 0U;
        size_t       fraction     = 0U;

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
        for (unsigned int byte_idx = 0U; (byte_idx < blk_sz) && (*allocated_num < required_alloc_num); byte_idx++) {
            for (unsigned int bit_idx = 0U; (bit_idx < 8U) && (*allocated_num < required_alloc_num); bit_idx++) {
                unsigned int data_idx = blk_idx * bits_per_bitmap + byte_idx * 8U + bit_idx;
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

    mofs_free(bitmap_buf);
    return ret;
}

/**
 * @brief Allocate data blocks for a file by appending to the block list.
 *
 * Function behavior:
 * - Reads inode and rejects when `i_nr_blocks + req_blk_num` exceeds
 *   `MOFS_MAX_FILE_DATA_BLOCKS`.
 * - Exports the current mapping via `export_data_block_list()`, appends new block numbers.
 * - Rebuilds the on-disk list via `rebuild_data_block_list()` (new list nodes as needed).
 * - Writes the inode, then frees the previous list-node chain on success.
 * - On failure, rolls back new data bitmap bits and any provisional list chain.
 *
 * @param[in] inode_num Target inode number.
 * @param[in] req_blk_num Number of data blocks to append at the logical end.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOSPC if file block limit or free space is insufficient.
 * @return Non-zero errno value propagated from inode/block operations.
 */
int allocate_data_block(int inode_num, unsigned int req_blk_num)
{
    int           ret = 0;
    mofs_inode_t  inode_buf;
    unsigned int *flat_abs     = NULL;
    unsigned int *new_data_idx = NULL;
    unsigned int  new_data_num = 0U;
    unsigned int  new_total    = 0U;

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

    /* Reject when the new total number of data blocks exceeds the limit. */
    if ((inode_buf.i_nr_blocks + req_blk_num) > MOFS_MAX_FILE_DATA_BLOCKS) {
        return MOFS_ENOSPC;
    }

    /* Allocate memory for the new data block numbers and the new total number of data blocks. */
    new_total    = inode_buf.i_nr_blocks + req_blk_num;
    flat_abs     = (unsigned int *)mofs_malloc(new_total * sizeof(unsigned int));
    new_data_idx = (unsigned int *)mofs_malloc(req_blk_num * sizeof(unsigned int));
    if ((flat_abs == NULL) || (new_data_idx == NULL)) {
        ret = get_errno();
        goto out;
    }

    /* Copy existing mapping, then append new absolute block numbers at the tail. */
    if (inode_buf.i_nr_blocks > 0U) {
        ret = export_data_block_list(&inode_buf, flat_abs, inode_buf.i_nr_blocks);
        if (ret != 0) {
            goto out;
        }
    }

    ret = find_free_data_block_indices(new_data_idx, req_blk_num, &new_data_num);
    if (ret != 0) {
        goto out;
    }

    for (unsigned int i = 0U; i < new_data_num; i++) {
        ret = set_data_bitmap_bit(new_data_idx[i], true);
        if (ret != 0) {
            update_data_bitmap_bits(new_data_idx, i, false);
            goto out;
        }
        flat_abs[inode_buf.i_nr_blocks + i] = ctx.sp_blk.data_region_start + new_data_idx[i];
    }

    {
        unsigned int old_head = 0U;

        /* Write new list nodes; old chain is kept until inode is persisted. */
        ret = rebuild_data_block_list(&inode_buf, flat_abs, new_total, &old_head);
        if (ret != 0) {
            update_data_bitmap_bits(new_data_idx, new_data_num, false);
            goto out;
        }

        ret = mofs_write_inode(inode_num, &inode_buf);
        if (ret != 0) {
            (void)free_list_chain(inode_buf.i_data_head);
            update_data_bitmap_bits(new_data_idx, new_data_num, false);
            inode_buf.i_data_head = old_head;
            inode_buf.i_nr_blocks -= req_blk_num;
            goto out;
        }

        /* On-disk inode now references the new list; release obsolete list nodes. */
        if (old_head != 0U) {
            (void)free_list_chain(old_head);
        }
    }

out:
    mofs_free(flat_abs);
    mofs_free(new_data_idx);
    return ret;
}

/**
 * @brief Free file data blocks and compact the block list mapping.
 *
 * Function behavior:
 * - Exports the inode mapping via `export_data_block_list()` to a temporary array.
 * - Clears data bitmap bits for the range
 *   `[start_blk_num, start_blk_num + free_blk_num)`.
 * - Left-shifts surviving entries to preserve logical index continuity.
 * - Rebuilds list nodes and writes the inode; frees the superseded list chain.
 * - Restores data bitmap bits if list rebuild or inode write fails.
 *
 * @param[in] inode_num Target inode number.
 * @param[in] start_blk_num File-relative start index to free.
 * @param[in] req_blk_num Number of blocks to free from `start_blk_num`.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments or range are invalid.
 * @return MOFS_EIO if inode mapping is invalid.
 * @return Non-zero errno value propagated from inode/block operations.
 */
int free_data_block(int inode_num, unsigned int start_blk_num, unsigned int req_blk_num)
{
    int           ret = 0;
    mofs_inode_t  inode_buf;
    unsigned int *flat_abs     = NULL;
    unsigned int *compact_abs  = NULL;
    unsigned int  free_blk_num = 0U;
    unsigned int  remain_num   = 0U;
    unsigned int  saved_head   = 0U;
    unsigned int  saved_nr     = 0U;

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

    if (start_blk_num >= inode_buf.i_nr_blocks) {
        return MOFS_EINVAL;
    }

    free_blk_num = req_blk_num;
    if ((start_blk_num + free_blk_num) > inode_buf.i_nr_blocks) {
        free_blk_num = inode_buf.i_nr_blocks - start_blk_num;
    }

    remain_num = inode_buf.i_nr_blocks - free_blk_num;
    if (inode_buf.i_nr_blocks > 0U) {
        flat_abs = (unsigned int *)mofs_malloc(inode_buf.i_nr_blocks * sizeof(unsigned int));
        if (flat_abs == NULL) {
            return get_errno();
        }
        ret = export_data_block_list(&inode_buf, flat_abs, inode_buf.i_nr_blocks);
        if (ret != 0) {
            mofs_free(flat_abs);
            return ret;
        }
    }

    /* Release file data blocks in the requested logical index range. */
    for (unsigned int i = 0U; i < free_blk_num; i++) {
        unsigned int data_idx = 0U;

        ret = abs_to_data_idx(flat_abs[start_blk_num + i], &data_idx);
        if (ret != 0) {
            mofs_free(flat_abs);
            return ret;
        }
        ret = set_data_bitmap_bit(data_idx, false);
        if (ret != 0) {
            mofs_free(flat_abs);
            return ret;
        }
    }

    saved_head = inode_buf.i_data_head;
    saved_nr   = inode_buf.i_nr_blocks;

    /* Compact: indices before start_blk_num unchanged, tail shifted left. */
    if (remain_num > 0U) {
        compact_abs = (unsigned int *)mofs_malloc(remain_num * sizeof(unsigned int));
        if (compact_abs == NULL) {
            ret = get_errno();
            goto rollback_bitmap;
        }
        for (unsigned int i = 0U; i < remain_num; i++) {
            if (i < start_blk_num) {
                compact_abs[i] = flat_abs[i];
            } else {
                compact_abs[i] = flat_abs[i + free_blk_num];
            }
        }
    }

    {
        unsigned int old_head = 0U;

        ret = rebuild_data_block_list(&inode_buf, compact_abs, remain_num, &old_head);
        if (ret != 0) {
            goto rollback_bitmap;
        }

        ret = mofs_write_inode(inode_num, &inode_buf);
        if (ret != 0) {
            (void)free_list_chain(inode_buf.i_data_head);
            inode_buf.i_data_head = saved_head;
            inode_buf.i_nr_blocks = saved_nr;
            goto rollback_bitmap;
        }

        if (old_head != 0U) {
            (void)free_list_chain(old_head);
        }
    }

    mofs_free(flat_abs);
    mofs_free(compact_abs);
    return 0;

rollback_bitmap:
    /* Restore data bitmap bits cleared before rebuild/write failed. */
    for (unsigned int i = 0U; i < free_blk_num; i++) {
        unsigned int data_idx = flat_abs[start_blk_num + i] - ctx.sp_blk.data_region_start;
        (void)set_data_bitmap_bit(data_idx, true);
    }
    mofs_free(flat_abs);
    mofs_free(compact_abs);
    return ret;
}

/**
 * @brief Read exactly one filesystem block from the current device offset.
 *
 * Function behavior:
 * - Calls `dev_read()` for `ctx.sp_blk.blk_size` bytes.
 * - Converts a low-level read error (`-1`) into `0` and reports the error
 *   code through `err`.
 *
 * @param[in] fd Device file descriptor.
 * @param[out] buf Destination buffer for one block.
 * @param[out] err Error code storage. Set to 0 on no low-level read error.
 * @return Number of bytes read (full block or a short read).
 * @return 0 when `dev_read()` fails with `-1` (details are stored in `*err`).
 */
static int read_one_block(int fd, void *buf, int *err)
{
    size_t const nb = mofs_io_blk_sz();
    int          ret;

    if (nb == 0U) {
        *err = MOFS_EINVAL;
        return 0;
    }

    ret = dev_read(fd, buf, nb);

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
    int          err       = 0;
    int          ret       = 0;
    size_t const blk_bytes = mofs_io_blk_sz();
    uint64_t     byte_off;

    if ((fd < 0) || (buf == NULL) || (read_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    if ((ret == 0) && (blk_bytes == 0U)) {
        ret = MOFS_EINVAL;
    }

    /* Seek to start block */
    if (ret == 0) {
        byte_off = (uint64_t)start_blk_num * (uint64_t)blk_bytes;
    }

    if (ret == 0) {
        off_t offset = dev_lseek(fd, (off_t)byte_off, MOFS_SEEK_SET);
        if (offset < 0) {
            ret = get_errno();
        } else if (((uint64_t)offset % (uint64_t)blk_bytes) != 0ULL) {
            ret = MOFS_EINVAL;
        }
    }

    if (ret == 0) {
        *fraction     = 0U;
        *read_blk_num = 0U;

        for (unsigned int i = 0U; i < req_blk_num; i++) {
            ret = read_one_block(fd, buf, &err);
            if (ret == 0) {
                ret = err;
                break;
            } else if ((size_t)ret != blk_bytes) {
                *fraction = (size_t)ret;
                ret       = 0;
                break;
            } else {
                ret = 0;
            }
            *read_blk_num = *read_blk_num + 1;
            buf           = (char *)buf + blk_bytes;
        }
    }

    return ret;
}

/**
 * @brief Write exactly one filesystem block at the current device offset.
 *
 * Function behavior:
 * - Calls `dev_write()` for `ctx.sp_blk.blk_size` bytes.
 * - Converts a low-level write error (`-1`) into `0` and reports the error
 *   code through `err`.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] buf Source buffer containing one block to write.
 * @param[out] err Error code storage. Set to 0 on no low-level write error.
 * @return Number of bytes written (full block or a short write).
 * @return 0 when `dev_write()` fails with `-1` (details are stored in `*err`).
 */
static int write_one_block(int fd, const void *buf, int *err)
{
    size_t const nb = mofs_io_blk_sz();
    int          ret;

    if (nb == 0U) {
        *err = MOFS_EINVAL;
        return 0;
    }

    ret = dev_write(fd, buf, nb);

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
    int          err       = 0;
    int          ret       = 0;
    size_t const blk_bytes = mofs_io_blk_sz();
    uint64_t     byte_off;

    if ((fd < 0) || (buf == NULL) || (written_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    if ((ret == 0) && (blk_bytes == 0U)) {
        ret = MOFS_EINVAL;
    }

    /* Align check */
    if (ret == 0) {
        byte_off     = (uint64_t)start_blk_num * (uint64_t)blk_bytes;
        off_t offset = dev_lseek(fd, (off_t)byte_off, MOFS_SEEK_SET);
        if (offset < 0) {
            ret = get_errno();
        } else if (((uint64_t)offset % (uint64_t)blk_bytes) != 0ULL) {
            ret = MOFS_EINVAL;
        }
    }

    if (ret == 0) {
        *fraction        = 0U;
        *written_blk_num = 0U;

        for (unsigned int i = 0U; i < req_blk_num; i++) {
            ret = write_one_block(fd, buf, &err);
            if (ret == 0) {
                ret = err;
                break;
            } else if ((size_t)ret != blk_bytes) {
                *fraction = (size_t)ret;
                ret       = 0;
                break;
            } else {
                ret = 0;
            }
            *written_blk_num = *written_blk_num + 1;
            buf              = (char *)buf + blk_bytes;
        }
    }

    return ret;
}
