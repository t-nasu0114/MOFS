#include "mofs_core_util.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_type.h>

/**
 * @brief Read one data block of a file specified by inode number.
 *
 * Function behavior:
 * - Reads inode metadata for the target file.
 * - Validates requested file block index (`start_blk_num`) against file size.
 * - Reads one filesystem block from the file's data-block mapping.
 * - Returns the valid byte count in the last file block through `fraction`.
 *
 * @param[in] inode_num Target inode number.
 * @param[out] buf Destination buffer for one block of file data.
 * @param[in] start_blk_num File-relative data block index to read.
 * @param[out] fraction Valid bytes in the returned block when reading the
 *                      last partial block; otherwise 0.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid or the block index is out of
 *         file range.
 * @return Non-zero errno value from `get_errno()` on inode/disk read failures.
 */
int read_file_data_block(int inode_num, void *buf, unsigned int start_blk_num, size_t *fraction)
{
    int          ret = 0;
    mofs_inode_t inode_buf;
    unsigned int read_blk_num = 0;
    unsigned int abs_blk_num  = 0;

    if ((inode_num < 0) || (buf == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    if (start_blk_num >= MOFS_DATA_BLK_PER_FILE) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        ret = mofs_read_inode(inode_num, &inode_buf);
        if (ret == 0) {
            if (((inode_buf.i_size + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE) <= start_blk_num) {
                ret = MOFS_EINVAL;
            }
        }
    }

    if (ret == 0) {
        *fraction = 0;

        abs_blk_num = inode_buf.i_data_blk[start_blk_num];
        ret         = read_continuous_blocks(ctx.dev_fd, buf, 1, abs_blk_num, &read_blk_num, fraction);
        if ((read_blk_num != 1) || (*fraction != 0)) {
            if (ret == 0) {
                ret = get_errno();
            }
        } else {
            if (inode_buf.i_size / MOFS_BLK_SIZE <= start_blk_num) {
                (*fraction) = inode_buf.i_size % MOFS_BLK_SIZE;
            } else {
                (*fraction) = 0;
            }
        }
    }

    return ret;
}
