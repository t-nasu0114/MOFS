#include "mofs_util.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_mem.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Read an inode entry from the on-disk inode table.
 *
 * Function behavior:
 * - Computes the inode-table block offset and in-block inode offset.
 * - Reads one inode-table block from disk.
 * - Copies the target inode entry into the caller-provided structure.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] inode_num Inode number to read (zero-based index in table).
 * @param[out] inode Destination pointer to receive inode data.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return Non-zero errno value from `get_errno()` on seek/read/allocation
 *         failures.
 */
int mofs_read_inode(int fd, int inode_num, mofs_inode_t *inode)
{
    int          ret          = 0;
    off_t        blk_offset   = 0;
    off_t        inode_offset = 0;
    unsigned int read_blk_num = 0;
    size_t       fraction     = 0;
    void        *buf          = NULL;
    void        *inode_ptr    = NULL;

    if (fd < 0 || inode_num < 0 || inode == NULL) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        buf = mofs_malloc(MOFS_BLK_SIZE);
        if (buf == NULL) {
            ret = get_errno();
        }
    }

    blk_offset = (ctx.sp_blk.inode_table_start * MOFS_BLK_SIZE);
    blk_offset += (inode_num * sizeof(mofs_inode_t)) / MOFS_BLK_SIZE;
    inode_offset = (inode_num * sizeof(mofs_inode_t)) % MOFS_BLK_SIZE;

    if (ret == 0) {
        if (dev_lseek(fd, blk_offset, MOFS_SEEK_SET) < 0) {
            ret = get_errno();
        }
    }

    if (ret == 0) {
        ret = read_continuous_blocks(fd, buf, 1, &read_blk_num, &fraction);
        if ((ret != 0) || (read_blk_num != 1) || (fraction != 0)) {
            ret = get_errno();
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
 * @brief Resolve a filesystem path to an inode number.
 *
 * Function behavior:
 * - Validates the input path format.
 * - Currently supports only the root path (`"/"`), which is mapped to inode 2.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[out] inode_num Destination pointer for the resolved inode number.
 * @return 0 on success.
 * @return MOFS_EINVAL if the path is invalid or unsupported.
 */
int mofs_path_to_inode_num(const char *path, int *inode_num)
{
    int ret    = 0;
    *inode_num = 0;

    if ((path == NULL) || (path[0] != '/')) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        if ((path[0] == '/') && (path[1] == '\0')) {
            *inode_num = 2;
        } else {
            ret = MOFS_EINVAL;
        }
    }

    return ret;
}