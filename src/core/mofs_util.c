
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <stddef.h>
#include <string.h>

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
        if ((dev_lseek(fd, start_blk_num * MOFS_BLK_SIZE, MOFS_SEEK_SET) < 0)) {
            ret = get_errno();
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
static int write_one_block(int fd, void *buf, int *err)
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
 * @brief Write multiple contiguous filesystem blocks at the current offset.
 *
 * Function behavior:
 * - Validates arguments and block alignment of the current file position.
 * - Repeatedly writes one block until `blk_num` blocks are written, a short
 *   write occurs, or an error is detected.
 * - Updates the number of full blocks written and the short-write remainder.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] buf Source buffer containing contiguous blocks.
 * @param[in] blk_num Number of blocks requested to write.
 * @param[out] written_blk_num Number of full blocks successfully written.
 * @param[out] fraction Number of bytes for a short write in the last attempt.
 * @return 0 on success (including short-write case; see `fraction`).
 * @return MOFS_EINVAL if arguments are invalid or offset is not block-aligned.
 * @return Non-zero errno value from `get_errno()` on write-related failures.
 */
int write_continuous_blocks(int fd, void *buf, unsigned int blk_num, unsigned int *written_blk_num, size_t *fraction)
{
    int err = 0;
    int ret = 0;

    if ((fd < 0) || (buf == NULL) || (written_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
    }

    /* Align check */
    if ((dev_lseek(fd, 0, MOFS_SEEK_CUR) % MOFS_BLK_SIZE) != 0) {
        ret = MOFS_EINVAL;
    }

    *fraction        = 0U;
    *written_blk_num = 0U;

    if (ret == 0) {
        for (int i = 0; i < blk_num; i++) {
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

    return ret;
}

/**
 * @brief Find a named directory entry under a parent directory inode.
 *
 * Function behavior:
 * - Reads the parent directory inode and iterates its data blocks.
 * - Scans directory entries to find the entry matching `component`.
 * - Returns the matched child inode number when found.
 *
 * @param[in] component Directory entry name to search.
 * @param[in] parent_inode_num Parent directory inode number.
 * @param[out] child_inode_num Destination pointer for matched child inode.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOENT if the entry is not found.
 * @return Non-zero errno value from `get_errno()` on memory or read failures.
 */
int find_dir_entry(char *component, int parent_inode_num, int *child_inode_num)
{
    int          ret            = 0;
    int          parent_blk_num = 0;
    bool         found          = false;
    void        *buf            = NULL;
    size_t       fraction       = 0;
    mofs_inode_t inode_buf;

    if ((component == NULL) || (parent_inode_num < 0) || (child_inode_num == NULL)) {
        ret = MOFS_EINVAL;
        goto out1;
    }

    if ((buf = mofs_malloc(MOFS_BLK_SIZE)) == NULL) {
        ret = get_errno();
        goto out1;
    }

    ret = mofs_read_inode(parent_inode_num, &inode_buf);
    if (ret != 0) {
        goto out2;
    }

    parent_blk_num = (inode_buf.i_size + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE;

    for (int i = 0; i < parent_blk_num; i++) {
        ret = read_file_data_block(parent_inode_num, buf, i, &fraction);
        if ((ret != 0) && (fraction == 0)) {
            break;
        } else {
            mofs_dirent_t *dirent = (mofs_dirent_t *)buf;

            unsigned int dirent_num;
            if (fraction != 0) {
                dirent_num = fraction / sizeof(mofs_dirent_t);
            } else {
                dirent_num = MOFS_BLK_SIZE / sizeof(mofs_dirent_t);
            }

            for (int j = 0; j < dirent_num; j++) {
                if (strcmp(dirent[j].name, component) == 0) {
                    *child_inode_num = dirent[j].inode_num;
                    found            = true;
                    break;
                }
                dirent++;
            }

            if (found == true) {
                break;
            }
        }
    }

    if ((ret == 0) && (found == false)) {
        ret = MOFS_ENOENT;
    }

out2:
    if (buf != NULL) {
        mofs_free(buf);
    }
out1:
    return ret;
}