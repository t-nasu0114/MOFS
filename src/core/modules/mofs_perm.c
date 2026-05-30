#include <mofs_errno.h>
#include <mofs_perm.h>

/**
 * @brief Verify caller access to an inode for the given open flags.
 *
 * Function behavior:
 * - Validates arguments and open-flag combinations (`MOFS_OFLAG_ACCMODE`).
 * - Maps caller uid/gid to owner/group/other permission bits.
 * - Checks read, write, execute, or search permission against `i_mode`.
 *
 * @param[in] flags Open flags (`MOFS_OFLAG_*` access mode bits).
 * @param[in] user Caller user context.
 * @param[in] inode Target inode metadata.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments or flag combination is invalid.
 * @return MOFS_EACCES if permission is denied.
 */
int mofs_check_open_permission(int flags, const mofs_user_ctx_t *user, const mofs_inode_t *inode)
{
    int          ret             = 0;
    unsigned int open_perm_flags = 0;
    bool         is_determined   = false;

    if ((user == NULL) || (inode == NULL)) {
        ret = MOFS_EINVAL;
        goto out;
    }

    if ((flags & MOFS_OFLAG_ACCMODE) == 0) {
        ret = MOFS_EINVAL;
        goto out;
    }

    if ((flags & MOFS_OFLAG_EXEC) == MOFS_OFLAG_EXEC) {
        open_perm_flags = MOFS_OFLAG_EXEC;
        is_determined   = true;
    }
    if ((flags & MOFS_OFLAG_SEARCH) == MOFS_OFLAG_SEARCH) {
        if (is_determined) {
            open_perm_flags = 0;
            ret             = MOFS_EINVAL;
            goto out;
        } else {
            open_perm_flags = MOFS_OFLAG_SEARCH;
            is_determined   = true;
        }
    }
    if ((flags & MOFS_OFLAG_RDONLY) == MOFS_OFLAG_RDONLY) {
        if (is_determined) {
            open_perm_flags = 0;
            ret             = MOFS_EINVAL;
            goto out;
        } else {
            open_perm_flags = MOFS_OFLAG_RDONLY;
            is_determined   = true;
        }
    }
    if ((flags & MOFS_OFLAG_WRONLY) == MOFS_OFLAG_WRONLY) {
        if (is_determined) {
            if (open_perm_flags == MOFS_OFLAG_RDONLY) {
                open_perm_flags = MOFS_OFLAG_RDWR;
            } else {
                open_perm_flags = 0;
                ret             = MOFS_EINVAL;
                goto out;
            }
        } else {
            open_perm_flags = MOFS_OFLAG_WRONLY;
            is_determined   = true;
        }
    }

    if (is_determined) {
        bool         is_member  = false;
        unsigned int is_read    = 0;
        unsigned int is_write   = 0;
        unsigned int is_execute = 0;

        ret = mofs_is_caller_in_group(inode->i_gid, &is_member);
        if (ret != 0) {
            goto out;
        }
        if (user->uid == inode->i_uid) {
            is_read    = MOFS_S_IRUSR;
            is_write   = MOFS_S_IWUSR;
            is_execute = MOFS_S_IXUSR;
        } else if (is_member) {
            is_read    = MOFS_S_IRGRP;
            is_write   = MOFS_S_IWGRP;
            is_execute = MOFS_S_IXGRP;
        } else {
            is_read    = MOFS_S_IROTH;
            is_write   = MOFS_S_IWOTH;
            is_execute = MOFS_S_IXOTH;
        }

        if (open_perm_flags == MOFS_OFLAG_RDONLY) {
            if ((inode->i_mode & is_read) == 0U) {
                ret = MOFS_EACCES;
            }
        } else if (open_perm_flags == MOFS_OFLAG_WRONLY) {
            if ((inode->i_mode & is_write) == 0U) {
                ret = MOFS_EACCES;
            }
        } else if (open_perm_flags == MOFS_OFLAG_RDWR) {
            if (((inode->i_mode & is_read) == 0U) || ((inode->i_mode & is_write) == 0U)) {
                ret = MOFS_EACCES;
            }
        } else if (open_perm_flags == MOFS_OFLAG_EXEC) {
            if ((inode->i_mode & is_execute) == 0U) {
                ret = MOFS_EACCES;
            }
        } else if (open_perm_flags == MOFS_OFLAG_SEARCH) {
            if ((inode->i_mode & is_execute) == 0U) {
                ret = MOFS_EACCES;
            }
        }
    }

out:
    return ret;
}

/**
 * @brief Verify directory traverse (search) permission for a caller.
 *
 * @param[in] user Caller user context.
 * @param[in] dir_inode Directory inode metadata.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EACCES if traverse permission is denied.
 */
int mofs_check_dir_traverse(const mofs_user_ctx_t *user, const mofs_inode_t *dir_inode)
{
    return mofs_check_open_permission(MOFS_OFLAG_SEARCH, user, dir_inode);
}

/**
 * @brief Verify directory read permission for a caller.
 *
 * @param[in] user Caller user context.
 * @param[in] dir_inode Directory inode metadata.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EACCES if read permission is denied.
 */
int mofs_check_dir_read(const mofs_user_ctx_t *user, const mofs_inode_t *dir_inode)
{
    return mofs_check_open_permission(MOFS_OFLAG_RDONLY, user, dir_inode);
}

/**
 * @brief Verify directory write and traverse permission for a caller.
 *
 * @param[in] user Caller user context.
 * @param[in] dir_inode Directory inode metadata.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EACCES if write or traverse permission is denied.
 */
int mofs_check_dir_write(const mofs_user_ctx_t *user, const mofs_inode_t *dir_inode)
{
    int ret = 0;

    ret = mofs_check_open_permission(MOFS_OFLAG_WRONLY, user, dir_inode);
    if (ret != 0) {
        return ret;
    }
    return mofs_check_dir_traverse(user, dir_inode);
}
