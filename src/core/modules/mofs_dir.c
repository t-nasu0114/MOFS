#include "mofs_core_util.h"
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_str.h>
#include <mofs_type.h>

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
                if (mofs_strcmp(dirent[j].name, component) == 0) {
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
