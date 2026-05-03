#include "mofs_block.h"
#include "mofs_core.h"
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_path.h>
#include <mofs_str.h>
#include <mofs_type.h>
#include <mofs_user.h>
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

static int check_dir_write_permission(const mofs_user_ctx_t *user, const mofs_inode_t *inode)
{
    int          ret       = 0;
    bool         is_member = false;
    unsigned int write_bit = 0U;

    if ((user == NULL) || (inode == NULL)) {
        return MOFS_EINVAL;
    }

    ret = mofs_is_caller_in_group(inode->i_gid, &is_member);
    if (ret != 0) {
        return ret;
    }

    if (user->uid == inode->i_uid) {
        write_bit = MOFS_S_IWUSR;
    } else if (is_member) {
        write_bit = MOFS_S_IWGRP;
    } else {
        write_bit = MOFS_S_IWOTH;
    }

    if ((inode->i_mode & write_bit) == 0U) {
        return MOFS_EACCES;
    }

    return 0;
}

static int write_dot_entries(int child_inode_num, int parent_inode_num)
{
    int            ret             = 0;
    mofs_dirent_t *blk_buf         = NULL;
    unsigned int   written_blk_num = 0U;
    size_t         fraction        = 0U;

    blk_buf = (mofs_dirent_t *)mofs_malloc(MOFS_BLK_SIZE);
    if (blk_buf == NULL) {
        return get_errno();
    }
    mofs_memset(blk_buf, 0, MOFS_BLK_SIZE);

    mofs_strcpy(blk_buf[0].name, ".");
    blk_buf[0].inode_num = (uint32_t)child_inode_num;
    mofs_strcpy(blk_buf[1].name, "..");
    blk_buf[1].inode_num = (uint32_t)parent_inode_num;

    ret = write_file_data_block(child_inode_num, blk_buf, 0U, 1U, &written_blk_num, &fraction);
    if (ret != 0) {
        mofs_free(blk_buf);
        return ret;
    }
    if ((written_blk_num != 1U) || (fraction != 0U)) {
        mofs_free(blk_buf);
        return MOFS_EIO;
    }

    mofs_free(blk_buf);
    return 0;
}

static bool is_dot_or_dotdot_name(const char *name)
{
    if (name == NULL) {
        return false;
    }

    return (mofs_strcmp(name, ".") == 0) || (mofs_strcmp(name, "..") == 0);
}

static int is_directory_empty(int inode_num, bool *is_empty)
{
    int            ret          = 0;
    mofs_inode_t   inode;
    mofs_dirent_t *buf          = NULL;
    unsigned int   read_blk_num = 0U;
    size_t         fraction     = 0U;
    bool           empty        = true;
    unsigned int   dir_blk_num  = 0U;

    if ((inode_num < 0) || (is_empty == NULL)) {
        return MOFS_EINVAL;
    }

    ret = mofs_read_inode(inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    if ((inode.i_mode & MOFS_FTYPE_DIR) == 0U) {
        return MOFS_ENOTDIR;
    }

    buf = (mofs_dirent_t *)mofs_malloc(MOFS_BLK_SIZE);
    if (buf == NULL) {
        return get_errno();
    }

    dir_blk_num = (inode.i_size + MOFS_BLK_SIZE - 1U) / MOFS_BLK_SIZE;
    for (unsigned int blk_idx = 0U; blk_idx < dir_blk_num; blk_idx++) {
        unsigned int dirent_num = 0U;
        ret = read_file_data_block(inode_num, buf, blk_idx, 1U, &read_blk_num, &fraction);
        if (ret != 0) {
            goto out;
        }
        if (!(((read_blk_num == 1U) && (fraction == 0U)) || ((read_blk_num == 0U) && (fraction != 0U)))) {
            ret = MOFS_EIO;
            goto out;
        }

        if (fraction != 0U) {
            dirent_num = (unsigned int)(fraction / sizeof(mofs_dirent_t));
        } else {
            dirent_num = MOFS_BLK_SIZE / sizeof(mofs_dirent_t);
        }

        for (unsigned int dir_idx = 0U; dir_idx < dirent_num; dir_idx++) {
            if ((buf[dir_idx].inode_num != 0U) && (buf[dir_idx].name[0] != '\0') &&
                !is_dot_or_dotdot_name(buf[dir_idx].name)) {
                empty = false;
                break;
            }
        }
        if (empty == false) {
            break;
        }
    }

out:
    mofs_free(buf);
    if (ret == 0) {
        *is_empty = empty;
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
    unsigned int read_blk_num = 0;

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
        ret = read_file_data_block(parent_inode_num, buf, i, 1, &read_blk_num, &fraction);
        if (ret != 0) {
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

/**
 * @brief Remove a named directory entry from a parent directory file.
 *
 * Function behavior:
 * - Reads parent inode and validates directory type.
 * - Scans directory data blocks to find an entry matching `component`.
 * - Marks matched entry as tombstone (`inode_num = 0`, empty name).
 * - Writes back the modified block without shrinking `i_size`.
 *
 * @param[in] component Directory entry name to remove.
 * @param[in] parent_inode_num Parent directory inode number.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOTDIR if parent inode is not a directory.
 * @return MOFS_ENOENT if target entry is not found.
 * @return MOFS_EIO if short read/write is detected.
 * @return Non-zero errno value propagated from inode/data I/O helpers.
 */
int remove_dir_entry(const char *component, int parent_inode_num)
{
    int          ret            = 0;
    int          parent_blk_num = 0;
    bool         found          = false;
    void        *buf            = NULL;
    size_t       fraction       = 0U;
    mofs_inode_t parent_inode;
    unsigned int read_blk_num    = 0U;
    unsigned int written_blk_num = 0U;

    if ((component == NULL) || (component[0] == '\0') || (parent_inode_num < 0)) {
        return MOFS_EINVAL;
    }

    buf = mofs_malloc(MOFS_BLK_SIZE);
    if (buf == NULL) {
        return get_errno();
    }

    ret = mofs_read_inode(parent_inode_num, &parent_inode);
    if (ret != 0) {
        goto out;
    }
    if ((parent_inode.i_mode & MOFS_FTYPE_DIR) == 0U) {
        ret = MOFS_ENOTDIR;
        goto out;
    }

    /* Scan parent directory blocks and tombstone the first matched entry. */
    parent_blk_num = (parent_inode.i_size + MOFS_BLK_SIZE - 1U) / MOFS_BLK_SIZE;
    for (int blk_idx = 0; blk_idx < parent_blk_num; blk_idx++) {
        unsigned int dirent_num = 0U;

        /* Read one directory block. */
        ret = read_file_data_block(parent_inode_num, buf, (unsigned int)blk_idx, 1U, &read_blk_num, &fraction);
        if (ret != 0) {
            break;
        }
        if (!(((read_blk_num == 1U) && (fraction == 0U)) || ((read_blk_num == 0U) && (fraction != 0U)))) {
            ret = MOFS_EIO;
            break;
        }

        if (fraction != 0U) {
            dirent_num = (unsigned int)(fraction / sizeof(mofs_dirent_t));
        } else {
            dirent_num = MOFS_BLK_SIZE / sizeof(mofs_dirent_t);
        }
        /* Find target name and clear the slot as tombstone. */
        for (unsigned int dir_idx = 0U; dir_idx < dirent_num; dir_idx++) {
            mofs_dirent_t *dirent = &((mofs_dirent_t *)buf)[dir_idx];
            if ((dirent->inode_num != 0U) && (mofs_strcmp(dirent->name, component) == 0)) {
                mofs_memset(dirent, 0, sizeof(mofs_dirent_t));
                found = true;
                break;
            }
        }

        if (found == true) {
            /* Persist the modified block; directory i_size is unchanged. */
            ret = write_file_data_block(parent_inode_num, buf, (unsigned int)blk_idx, 1U, &written_blk_num, &fraction);
            if (ret != 0) {
                break;
            }
            if ((written_blk_num != 1U) || (fraction != 0U)) {
                ret = MOFS_EIO;
            }
            break;
        }
    }

    if ((ret == 0) && (found == false)) {
        ret = MOFS_ENOENT;
    }

out:
    if (buf != NULL) {
        mofs_free(buf);
    }
    return ret;
}

/**
 * @brief Add a directory entry to a parent directory file.
 *
 * Function behavior:
 * - Validates parent directory and requested entry parameters.
 * - Reuses a tombstone slot when available.
 * - Appends a new slot at EOF when no reusable slot exists.
 * - Extends directory data blocks and `i_size` when append needs more space.
 *
 * @param[in] component Directory entry name to add.
 * @param[in] parent_inode_num Parent directory inode number.
 * @param[in] child_inode_num Child inode number to store.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOTDIR if parent inode is not a directory.
 * @return MOFS_EEXIST if the same valid entry already exists.
 * @return MOFS_EFBIG if directory slot capacity is exceeded.
 * @return MOFS_EIO if short read/write is detected.
 * @return Non-zero errno value propagated from inode/data/block helpers.
 */
int add_dir_entry(const char *component, int parent_inode_num, int child_inode_num)
{
    int            ret              = 0;
    mofs_inode_t   parent_inode;
    mofs_dirent_t *dirent_buf       = NULL;
    unsigned int   read_blk_num     = 0U;
    unsigned int   written_blk_num  = 0U;
    size_t         fraction         = 0U;
    size_t         name_len         = 0U;
    bool           found_reusable   = false;
    bool           allocated_newblk = false;
    unsigned int   reusable_idx     = 0U;
    unsigned int   entries_per_blk  = MOFS_BLK_SIZE / sizeof(mofs_dirent_t);
    unsigned int   append_entry_idx = 0U;
    unsigned int   target_entry_idx = 0U;
    unsigned int   target_blk_idx   = 0U;
    unsigned int   target_in_blk    = 0U;
    unsigned int   old_blk_num      = 0U;
    unsigned int   max_entry_num    = MOFS_DATA_BLK_PER_FILE * (MOFS_BLK_SIZE / sizeof(mofs_dirent_t));

    if ((component == NULL) || (component[0] == '\0') || (parent_inode_num < 0) || (child_inode_num <= 0) ||
        (ctx.sp_blk.inode_num <= (unsigned int)child_inode_num)) {
        return MOFS_EINVAL;
    }
    name_len = mofs_strlen(component);
    if ((name_len == 0U) || (name_len >= MOFS_FILENAME_LEN)) {
        return MOFS_EINVAL;
    }

    dirent_buf = (mofs_dirent_t *)mofs_malloc(MOFS_BLK_SIZE);
    if (dirent_buf == NULL) {
        return get_errno();
    }

    ret = mofs_read_inode(parent_inode_num, &parent_inode);
    if (ret != 0) {
        goto out;
    }
    if ((parent_inode.i_mode & MOFS_FTYPE_DIR) == 0U) {
        ret = MOFS_ENOTDIR;
        goto out;
    }

    if ((parent_inode.i_size % sizeof(mofs_dirent_t)) != 0U) {
        ret = MOFS_EIO;
        goto out;
    }

    old_blk_num      = (parent_inode.i_size + MOFS_BLK_SIZE - 1U) / MOFS_BLK_SIZE;
    append_entry_idx = parent_inode.i_size / sizeof(mofs_dirent_t);
    if (append_entry_idx >= max_entry_num) {
        ret = MOFS_EFBIG;
        goto out;
    }

    /* First pass: detect duplicate name and remember first reusable tombstone. */
    for (unsigned int blk_idx = 0U; blk_idx < old_blk_num; blk_idx++) {
        unsigned int dirent_num = 0U;
        ret = read_file_data_block(parent_inode_num, dirent_buf, blk_idx, 1U, &read_blk_num, &fraction);
        if (ret != 0) {
            goto out;
        }
        if (!(((read_blk_num == 1U) && (fraction == 0U)) || ((read_blk_num == 0U) && (fraction != 0U)))) {
            ret = MOFS_EIO;
            goto out;
        }

        if (fraction != 0U) {
            dirent_num = (unsigned int)(fraction / sizeof(mofs_dirent_t));
        } else {
            dirent_num = entries_per_blk;
        }

        for (unsigned int dir_idx = 0U; dir_idx < dirent_num; dir_idx++) {
            if ((dirent_buf[dir_idx].inode_num != 0U) && (dirent_buf[dir_idx].name[0] != '\0') &&
                (mofs_strcmp(dirent_buf[dir_idx].name, component) == 0)) {
                ret = MOFS_EEXIST;
                goto out;
            }
            if ((!found_reusable) &&
                ((dirent_buf[dir_idx].inode_num == 0U) || (dirent_buf[dir_idx].name[0] == '\0'))) {
                found_reusable = true;
                reusable_idx   = blk_idx * entries_per_blk + dir_idx;
            }
        }
    }

    if (found_reusable) {
        target_entry_idx = reusable_idx;
    } else {
        target_entry_idx = append_entry_idx;
    }
    target_blk_idx = target_entry_idx / entries_per_blk;
    target_in_blk  = target_entry_idx % entries_per_blk;

    /* No tombstone slot: append at EOF, expanding by one data block if needed. */
    if (target_blk_idx >= old_blk_num) {
        if (old_blk_num >= MOFS_DATA_BLK_PER_FILE) {
            ret = MOFS_EFBIG;
            goto out;
        }
        ret = allocate_data_block(parent_inode_num, 1U);
        if (ret != 0) {
            goto out;
        }
        allocated_newblk = true;
        mofs_memset(dirent_buf, 0, MOFS_BLK_SIZE);
    } else {
        /* Reuse existing block that contains the chosen target slot. */
        ret = read_file_data_block(parent_inode_num, dirent_buf, target_blk_idx, 1U, &read_blk_num, &fraction);
        if (ret != 0) {
            goto rollback_alloc;
        }
        if (!(((read_blk_num == 1U) && (fraction == 0U)) || ((read_blk_num == 0U) && (fraction != 0U)))) {
            ret = MOFS_EIO;
            goto rollback_alloc;
        }
    }

    /* Write entry payload to selected slot. */
    mofs_memset(&dirent_buf[target_in_blk], 0, sizeof(mofs_dirent_t));
    mofs_strcpy(dirent_buf[target_in_blk].name, component);
    dirent_buf[target_in_blk].inode_num = (uint32_t)child_inode_num;

    ret = write_file_data_block(parent_inode_num, dirent_buf, target_blk_idx, 1U, &written_blk_num, &fraction);
    if (ret != 0) {
        goto rollback_alloc;
    }
    if (!((written_blk_num == 1U) && (fraction == 0U))) {
        ret = MOFS_EIO;
        goto rollback_alloc;
    }

    if (!found_reusable) {
        /* Only true append consumes logical size; tombstone reuse keeps size. */
        parent_inode.i_size += sizeof(mofs_dirent_t);
        ret = mofs_write_inode(parent_inode_num, &parent_inode);
        if (ret != 0) {
            goto rollback_alloc;
        }
    }

    ret = 0;
    goto out;

rollback_alloc:
    if (allocated_newblk) {
        /* Best-effort rollback for newly reserved block on append path failure. */
        (void)free_data_block(parent_inode_num, target_blk_idx, 1U);
    }

out:
    if (dirent_buf != NULL) {
        mofs_free(dirent_buf);
    }
    return ret;
}

/**
 * @brief Create a new directory and link it under parent directory.
 *
 * Function behavior:
 * - Resolves parent directory and target leaf component from path.
 * - Allocates and initializes child directory inode and one data block.
 * - Writes `.` and `..` entries into the first child directory block.
 * - Adds parent directory entry and updates parent link count.
 * - Performs best-effort rollback on failure.
 *
 * @param[in] path NULL-terminated absolute directory path.
 * @param[in] mode Permission bits for the new directory.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EEXIST if target path already exists.
 * @return MOFS_ENOTDIR if parent inode is not a directory.
 * @return MOFS_EPERM or MOFS_EACCES on permission failure.
 * @return Non-zero errno value propagated from lower helpers.
 */
int mofs_mkdir_core(const char *path, mode_t mode)
{
    int              ret                = 0;
    int              child_inode_num    = -1;
    bool             inode_allocated    = false;
    bool             block_allocated    = false;
    bool             parent_linked      = false;
    mofs_path_info_t path_info;
    mofs_user_ctx_t  user;
    mofs_inode_t     parent_inode;
    mofs_inode_t     child_inode;

    if ((path == NULL) || (path[0] != '/')) {
        return MOFS_EINVAL;
    }

    mofs_memset(&path_info, 0, sizeof(path_info));
    ret = mofs_resolve_path(path, MOFS_PATH_RESOLVE_PARENT | MOFS_PATH_RESOLVE_INODE | MOFS_PATH_ALLOW_MISSING_LEAF,
                            &path_info);
    if (ret != 0) {
        return ret;
    }
    if (path_info.leaf_found) {
        return MOFS_EEXIST;
    }

    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        return ret;
    }
    if (user.valid == false) {
        return MOFS_EPERM;
    }

    ret = mofs_read_inode(path_info.parent_inode_num, &parent_inode);
    if (ret != 0) {
        return ret;
    }
    if ((parent_inode.i_mode & MOFS_FTYPE_DIR) == 0U) {
        return MOFS_ENOTDIR;
    }
    ret = check_dir_write_permission(&user, &parent_inode);
    if (ret != 0) {
        return ret;
    }

    ret = allocate_inode(&child_inode_num);
    if (ret != 0) {
        goto rollback;
    }
    inode_allocated = true;

    mofs_memset(&child_inode, 0, sizeof(child_inode));
    child_inode.i_size  = 0U;
    child_inode.i_links = 2U;
    child_inode.i_mode  = (uint16_t)(MOFS_FTYPE_DIR | (mode & 0777U));
    child_inode.i_uid   = user.uid;
    child_inode.i_gid   = user.gid;
    ret = mofs_write_inode(child_inode_num, &child_inode);
    if (ret != 0) {
        goto rollback;
    }

    ret = allocate_data_block(child_inode_num, 1U);
    if (ret != 0) {
        goto rollback;
    }
    block_allocated = true;

    ret = write_dot_entries(child_inode_num, path_info.parent_inode_num);
    if (ret != 0) {
        goto rollback;
    }

    ret = mofs_read_inode(child_inode_num, &child_inode);
    if (ret != 0) {
        goto rollback;
    }
    child_inode.i_size = MOFS_BLK_SIZE;
    ret                = mofs_write_inode(child_inode_num, &child_inode);
    if (ret != 0) {
        goto rollback;
    }

    ret = add_dir_entry(path_info.leaf_name, path_info.parent_inode_num, child_inode_num);
    if (ret != 0) {
        goto rollback;
    }
    parent_linked = true;

    ret = mofs_read_inode(path_info.parent_inode_num, &parent_inode);
    if (ret != 0) {
        goto rollback;
    }
    parent_inode.i_links = parent_inode.i_links + 1U;
    ret                  = mofs_write_inode(path_info.parent_inode_num, &parent_inode);
    if (ret != 0) {
        goto rollback;
    }

    return 0;

rollback:
    if (parent_linked) {
        (void)remove_dir_entry(path_info.leaf_name, path_info.parent_inode_num);
    }
    if (block_allocated) {
        (void)free_data_block(child_inode_num, 0U, 1U);
    }
    if (inode_allocated) {
        (void)free_inode(child_inode_num);
    }
    return ret;
}

/**
 * @brief Remove an empty directory and unlink it from its parent.
 *
 * Function behavior:
 * - Resolves parent and target inodes from `path`.
 * - Validates that target is an empty directory (`.` and `..` only).
 * - Removes parent directory entry for target.
 * - Frees all child data blocks and inode.
 * - Decrements parent directory link count.
 *
 * @param[in] path NULL-terminated absolute directory path.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid or root/special name is requested.
 * @return MOFS_ENOTDIR if target inode is not a directory.
 * @return MOFS_ENOTEMPTY if target directory contains entries except `.` and `..`.
 * @return Non-zero errno value propagated from lower helpers.
 */
int mofs_rmdir_core(const char *path)
{
    int              ret          = 0;
    bool             is_empty     = false;
    unsigned int     used_blk_num = 0U;
    mofs_path_info_t path_info;
    mofs_inode_t     target_inode;
    mofs_inode_t     parent_inode;

    if ((path == NULL) || (path[0] != '/')) {
        return MOFS_EINVAL;
    }
    if ((path[1] == '\0') || is_dot_or_dotdot_name(path)) {
        return MOFS_EINVAL;
    }

    mofs_memset(&path_info, 0, sizeof(path_info));
    ret = mofs_resolve_path(path, MOFS_PATH_RESOLVE_PARENT | MOFS_PATH_RESOLVE_INODE, &path_info);
    if (ret != 0) {
        return ret;
    }
    if (is_dot_or_dotdot_name(path_info.leaf_name)) {
        return MOFS_EINVAL;
    }

    ret = mofs_read_inode(path_info.leaf_inode_num, &target_inode);
    if (ret != 0) {
        return ret;
    }
    if ((target_inode.i_mode & MOFS_FTYPE_DIR) == 0U) {
        return MOFS_ENOTDIR;
    }

    ret = is_directory_empty(path_info.leaf_inode_num, &is_empty);
    if (ret != 0) {
        return ret;
    }
    if (!is_empty) {
        return MOFS_ENOTEMPTY;
    }

    ret = remove_dir_entry(path_info.leaf_name, path_info.parent_inode_num);
    if (ret != 0) {
        return ret;
    }

    used_blk_num = (target_inode.i_size + MOFS_BLK_SIZE - 1U) / MOFS_BLK_SIZE;
    if (used_blk_num > 0U) {
        ret = free_data_block(path_info.leaf_inode_num, 0U, used_blk_num);
        if (ret != 0) {
            return ret;
        }
    }

    ret = free_inode(path_info.leaf_inode_num);
    if (ret != 0) {
        return ret;
    }

    ret = mofs_read_inode(path_info.parent_inode_num, &parent_inode);
    if (ret != 0) {
        return ret;
    }
    if (parent_inode.i_links == 0U) {
        return MOFS_EIO;
    }
    parent_inode.i_links = parent_inode.i_links - 1U;
    return mofs_write_inode(path_info.parent_inode_num, &parent_inode);
}

/**
 * @brief Open a directory and allocate an internal directory handle.
 *
 * Function behavior:
 * - Validates arguments and resolves `path` to an inode number.
 * - Verifies that the resolved inode is a directory.
 * - Allocates one entry from the global directory-handle pool and
 *   initializes cursor/cache fields for subsequent `readdir`.
 *
 * @param[in] path NULL-terminated absolute directory path.
 * @param[out] handle Destination pointer to receive an opened handle.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOTDIR if `path` does not point to a directory.
 * @return MOFS_ENFILE when no free handle entry is available.
 * @return Non-zero errno value propagated from path/inode resolution.
 */
int mofs_opendir_core(const char *path, mofs_dirhandle_t **handle)
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
            dirhandle_pool[index].dirent_buf    = (mofs_dirent_t){0};
            dirhandle_pool[index].used          = true;
        }
    }

    return ret;
}

/**
 * @brief Close an opened directory handle and release pool entry.
 *
 * Function behavior:
 * - Validates the handle pointer and opened state.
 * - Clears inode/cursor/cache fields and marks the pool slot unused.
 * - Sets caller handle pointer to NULL to prevent stale reuse.
 *
 * @param[in,out] handle Pointer to an opened directory handle pointer.
 * @return 0 on success.
 * @return MOFS_EINVAL if `handle` is invalid or not opened.
 */
int mofs_closedir_core(mofs_dirhandle_t **handle)
{
    int ret = 0;

    if ((handle == NULL) || (*handle == NULL) || ((*handle)->used == false)) {
        ret = MOFS_EINVAL;
    } else {
        (*handle)->inode_num     = 0;
        (*handle)->dirent_offset = 0;
        (*handle)->dirent_buf    = (mofs_dirent_t){0};
        (*handle)->used          = false;
        *handle                  = NULL;
    }
    return ret;
}

/**
 * @brief Read the next valid directory entry from an opened directory handle.
 *
 * Function behavior:
 * - Validates the handle and reads directory inode metadata.
 * - Scans directory data blocks from the current cursor position.
 * - Skips unused entries and stores the next valid entry in `dirent_buf`.
 * - Advances the cursor on success. At end-of-directory, stores a zeroed
 *   entry in `dirent_buf` and returns success (EOF is not an error).
 *
 * @param[in,out] handle Pointer to an opened directory handle pointer.
 * @return 0 on success (including EOF).
 * @return MOFS_EINVAL if `handle` is invalid.
 * @return MOFS_ENOTDIR if target inode is not a directory.
 * @return Non-zero errno value on memory or block-read failures.
 */
int mofs_readdir_core(mofs_dirhandle_t **handle)
{
    int            ret = 0;
    mofs_inode_t   inode;
    mofs_dirent_t *buf = NULL;
    mofs_dirent_t  dirent_tmp;
    size_t         fraction = 0;
    unsigned int   start_block;
    unsigned int   start_idx;
    unsigned int   entries_num;
    bool           found        = false;
    unsigned int   read_blk_num = 0;

    if ((handle == NULL) || (*handle == NULL) || ((*handle)->used == false)) {
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
                    ret = read_file_data_block((*handle)->inode_num, buf, start_block, 1, &read_blk_num, &fraction);

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
            mofs_memcpy(&(*handle)->dirent_buf, &dirent_tmp, sizeof(mofs_dirent_t));
            (*handle)->dirent_offset = start_block * (MOFS_BLK_SIZE / sizeof(mofs_dirent_t)) + start_idx + 1U;
        } else {
            /* EOF is not error.*/
            mofs_memset(&(*handle)->dirent_buf, 0, sizeof(mofs_dirent_t));
        }
    }

    if (buf != NULL) {
        mofs_free(buf);
    }

    return ret;
}
