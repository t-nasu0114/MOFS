#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_path.h>
#include <mofs_perm.h>
#include <mofs_str.h>
#include <mofs_user.h>

static int check_parent_traverse(bool check_access, const mofs_user_ctx_t *user, int parent_inode)
{
    int          ret = 0;
    mofs_inode_t inode;

    if (check_access == false) {
        return 0;
    }
    if (parent_inode == MOFS_ROOT_INODE_NUM) {
        return 0;
    }

    ret = mofs_read_inode(parent_inode, &inode);
    if (ret != 0) {
        return ret;
    }
    if ((inode.i_mode & MOFS_FTYPE_DIR) == 0U) {
        return MOFS_ENOTDIR;
    }
    return mofs_check_dir_traverse(user, &inode);
}

/**
 * @brief Resolve a path with configurable output details.
 *
 * Function behavior:
 * - Validates input path and requested resolve flags.
 * - Walks path components from root inode to the leaf parent.
 * - Optionally verifies traverse (search) permission on each parent directory.
 * - Optionally resolves the leaf inode and/or parent inode.
 * - Optionally tolerates missing leaf component.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[in] resolve_flags Combination of `MOFS_PATH_*` flags.
 * @param[out] path_info Resolve result storage.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments or resolve flags are invalid.
 * @return MOFS_EPERM if access check is requested and caller context is invalid.
 * @return MOFS_EACCES if traverse permission is denied on a path component.
 * @return MOFS_ENAMETOOLONG if leaf component exceeds directory entry limit.
 * @return MOFS_ENOENT if path resolution fails for existing-required component.
 * @return Non-zero errno value from memory allocation failures.
 */
int mofs_resolve_path(const char *path, unsigned int resolve_flags, mofs_path_info_t *path_info)
{
    int              ret          = 0;
    int              parent_inode = MOFS_ROOT_INODE_NUM;
    int              child_inode  = -1;
    char            *path_copy    = NULL;
    char            *current      = NULL;
    char            *next         = NULL;
    bool             require_leaf_inode;
    bool             require_parent;
    bool             allow_missing_leaf;
    bool             check_access;
    mofs_user_ctx_t  user;

    if ((path == NULL) || (path_info == NULL) || (path[0] != '/')) {
        return MOFS_EINVAL;
    }

    require_leaf_inode = ((resolve_flags & MOFS_PATH_RESOLVE_INODE) != 0U);
    require_parent     = ((resolve_flags & MOFS_PATH_RESOLVE_PARENT) != 0U);
    allow_missing_leaf = ((resolve_flags & MOFS_PATH_ALLOW_MISSING_LEAF) != 0U);
    check_access       = ((resolve_flags & MOFS_PATH_CHECK_ACCESS) != 0U);
    if ((!require_leaf_inode) && (!require_parent)) {
        return MOFS_EINVAL;
    }
    if (allow_missing_leaf && (!require_leaf_inode)) {
        return MOFS_EINVAL;
    }

    if (check_access) {
        ret = mofs_get_caller_user(&user);
        if (ret != 0) {
            return ret;
        }
        if (user.valid == false) {
            return MOFS_EPERM;
        }
    }

    mofs_memset(path_info, 0, sizeof(mofs_path_info_t));
    path_info->parent_inode_num = -1;
    path_info->leaf_inode_num   = -1;
    path_info->leaf_found       = 0;

    if (mofs_strcmp(path, "/") == 0) {
        if (require_parent) {
            return MOFS_EINVAL;
        }
        path_info->leaf_inode_num = MOFS_ROOT_INODE_NUM;
        path_info->leaf_found     = 1;
        return 0;
    }

    path_copy = mofs_malloc(mofs_strlen(path) + 1U);
    if (path_copy == NULL) {
        return get_errno();
    }
    mofs_strcpy(path_copy, path);

    current = mofs_strtok(path_copy, "/");
    if (current == NULL) {
        ret = MOFS_EINVAL;
        goto out;
    }

    while (current != NULL) {
        next = mofs_strtok(NULL, "/");
        if (next == NULL) {
            if (mofs_strlen(current) >= MOFS_FILENAME_LEN) {
                ret = MOFS_ENAMETOOLONG;
                break;
            }
            mofs_strcpy(path_info->leaf_name, current);
            if (require_parent) {
                path_info->parent_inode_num = parent_inode;
            }

            if (require_leaf_inode) {
                ret = check_parent_traverse(check_access, &user, parent_inode);
                if (ret != 0) {
                    break;
                }
                ret = find_dir_entry(current, parent_inode, &child_inode);
                if ((ret == 0) && (child_inode >= 0)) {
                    path_info->leaf_inode_num = child_inode;
                    path_info->leaf_found     = 1;
                } else if (allow_missing_leaf) {
                    ret = 0;
                } else {
                    ret = MOFS_ENOENT;
                }
            }
            break;
        }

        ret = check_parent_traverse(check_access, &user, parent_inode);
        if (ret != 0) {
            break;
        }
        ret = find_dir_entry(current, parent_inode, &child_inode);
        if ((ret != 0) || (child_inode < 0)) {
            ret = MOFS_ENOENT;
            break;
        }
        parent_inode = child_inode;
        current      = next;
    }

out:
    if (path_copy != NULL) {
        mofs_free(path_copy);
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
    int              ret       = 0;
    mofs_path_info_t path_info = {0};

    if (inode_num == NULL) {
        return MOFS_EINVAL;
    }

    ret = mofs_resolve_path(path, MOFS_PATH_RESOLVE_INODE | MOFS_PATH_CHECK_ACCESS, &path_info);
    if (ret == 0) {
        *inode_num = path_info.leaf_inode_num;
    }
    return ret;
}

/**
 * @brief Resolve a path into parent directory inode and final component name.
 *
 * Function behavior:
 * - Validates the input path format and output pointers.
 * - Traverses path components up to the parent directory.
 * - Copies the last path component into `component`.
 * - Returns parent directory inode number through `parent_inode_num`.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[out] parent_inode_num Parent directory inode number.
 * @param[out] component Final path component buffer.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid or path is root-only.
 * @return MOFS_ENAMETOOLONG if final component exceeds directory entry limit.
 * @return MOFS_ENOENT if any intermediate component is not found.
 * @return Non-zero errno value from memory allocation failures.
 */
int mofs_path_to_parent_and_component(const char *path, int *parent_inode_num, char *component)
{
    int              ret       = 0;
    mofs_path_info_t path_info = {0};

    if ((parent_inode_num == NULL) || (component == NULL)) {
        return MOFS_EINVAL;
    }

    ret = mofs_resolve_path(path, MOFS_PATH_RESOLVE_PARENT | MOFS_PATH_CHECK_ACCESS, &path_info);
    if (ret == 0) {
        *parent_inode_num = path_info.parent_inode_num;
        mofs_strcpy(component, path_info.leaf_name);
    }
    return ret;
}
