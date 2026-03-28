#include "mofs_core.h"
#include "mofs_core_util.h"
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_str.h>
#include <mofs_type.h>
#include <stdint.h>

/* Directory handle pool */
mofs_dirhandle_t dirhandle_pool[MOFS_DIRHANDLE_POOL_SIZE];

static int get_free_dirhandle_index(void)
{
    for (int i = 0; i < MOFS_DIRHANDLE_POOL_SIZE; i++) {
        if (!dirhandle_pool[i].used) {
            return i;
        }
    }
    return -1;
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

int mofs_dir_open(const char *path, mofs_dirhandle_t **handle)
{
    int          ret       = 0;
    int          inode_num = 0;
    mofs_inode_t inode;
    int          index = 0;

    if ((path == NULL) || (handle == NULL)) {
        ret = MOFS_EINVAL;
    } else {
        *handle = NULL;
    }

    if (ret == 0) {
        ret = mofs_path_to_inode_num(path, &inode_num);
        if (ret == 0) {
            mofs_memset(&inode, 0, sizeof(mofs_inode_t));
            ret = mofs_read_inode(inode_num, &inode);
            if (ret == 0) {
                if (inode.i_mode & MOFS_FTYPE_DIR) {
                    ret = 0;
                } else {
                    ret = MOFS_ENOTDIR;
                }
            }
        }
    }

    if (ret == 0) {
        index = get_free_dirhandle_index();
        if (index == -1) {
            ret = MOFS_ENFILE;
        } else {
            *handle                             = &dirhandle_pool[index];
            dirhandle_pool[index].inode_num     = inode_num;
            dirhandle_pool[index].dirent_offset = 0;
            dirhandle_pool[index].used          = true;
        }
    }

    return ret;
}

int mofs_dir_close(mofs_dirhandle_t **handle)
{
    int ret = 0;

    if ((handle == NULL) || (*handle == NULL) || ((*handle)->used == false)) {
        ret = MOFS_EINVAL;
    } else {
        (*handle)->inode_num     = 0;
        (*handle)->dirent_offset = 0;
        (*handle)->used          = false;
        *handle                  = NULL;
    }
    return ret;
}

int mofs_dir_read(mofs_dirhandle_t **handle, mofs_dirent_t *dirent)
{
    int            ret = 0;
    mofs_inode_t   inode;
    mofs_dirent_t *buf = NULL;
    mofs_dirent_t  dirent_tmp;
    size_t         fraction = 0;
    unsigned int   start_block;
    unsigned int   start_idx;
    unsigned int   entries_num;
    bool           found = false;

    if ((handle == NULL) || (*handle == NULL) || (dirent == NULL) || ((*handle)->used == false)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        buf = (mofs_dirent_t *)mofs_malloc(MOFS_BLK_SIZE);
        if (buf == NULL) {
            ret = get_errno();
        }
    }

    if (ret == 0) {
        ret = mofs_read_inode((*handle)->inode_num, &inode);
        if (ret == 0) {
            if (!(inode.i_mode & MOFS_FTYPE_DIR)) {
                ret = MOFS_ENOTDIR;
            } else {
                /* Read the directory data block */

                /* Calculate the start block and index */
                start_block = (*handle)->dirent_offset / (MOFS_BLK_SIZE / sizeof(mofs_dirent_t));
                start_idx   = (*handle)->dirent_offset % (MOFS_BLK_SIZE / sizeof(mofs_dirent_t));

                for (; start_block < (inode.i_size + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE; start_block++) {
                    /* Read the directory data block */
                    ret = read_file_data_block((*handle)->inode_num, buf, start_block, &fraction);

                    /* Find the directory entry in the buffer */
                    if (ret == 0) {
                        if (fraction != 0) {
                            entries_num = fraction / sizeof(mofs_dirent_t);
                        } else {
                            entries_num = MOFS_BLK_SIZE / sizeof(mofs_dirent_t);
                        }

                        for (; start_idx < entries_num; start_idx++) {
                            mofs_memcpy(&dirent_tmp, buf + start_idx, sizeof(mofs_dirent_t));
                            if ((dirent_tmp.inode_num != 0) && (dirent_tmp.name[0] != '\0')) {
                                found = true;
                                break;
                            }
                        }
                        /* Reset the index for the next block */
                        if (found == false) {
                            start_idx = 0;
                        }
                    } else {
                        break;
                    }

                    if (found == true) {
                        break;
                    }
                }
            }
        }
    }

    if (ret == 0) {
        if (found == true) {
            mofs_memcpy(dirent, &dirent_tmp, sizeof(mofs_dirent_t));
            (*handle)->dirent_offset = start_block * (MOFS_BLK_SIZE / sizeof(mofs_dirent_t)) + start_idx + 1U;
        } else {
            /* EOF is not error.*/
            mofs_memset(dirent, 0, sizeof(mofs_dirent_t));
        }
    }

    if (buf != NULL) {
        mofs_free(buf);
    }

    return ret;
}
