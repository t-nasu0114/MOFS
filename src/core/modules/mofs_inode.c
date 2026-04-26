#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_mem.h>
#include <mofs_type.h>

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

    if ((inode_num < 0) || (ctx.sp_blk.inode_num <= inode_num) || (inode == NULL)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        buf = mofs_malloc(MOFS_BLK_SIZE);
        if (buf == NULL) {
            ret = get_errno();
        }
    }

    blk_offset = ctx.sp_blk.inode_table_start;
    blk_offset += (inode_num * sizeof(mofs_inode_t)) / MOFS_BLK_SIZE;
    inode_offset = (inode_num * sizeof(mofs_inode_t)) % MOFS_BLK_SIZE;

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

    if ((inode_num < 0) || (ctx.sp_blk.inode_num <= inode_num) || (inode == NULL)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        blk_buf = mofs_malloc(MOFS_BLK_SIZE);
        if (blk_buf == NULL) {
            ret = get_errno();
        }
    }

    /* Copy inode to block buffer */
    if (ret == 0) {
        blk_offset = ctx.sp_blk.inode_table_start;
        blk_offset += (inode_num * sizeof(mofs_inode_t)) / MOFS_BLK_SIZE;
        inode_offset = (inode_num * sizeof(mofs_inode_t)) % MOFS_BLK_SIZE;

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
