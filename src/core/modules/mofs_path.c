#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_mem.h>
#include <mofs_path.h>
#include <mofs_str.h>

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
    int   ret              = 0;
    int   parent_inode_num = 2;
    int   child_inode_num  = -1;
    char *path_copy        = NULL;
    char *component        = NULL;

    if ((path == NULL) || (inode_num == NULL) || (path[0] != '/')) {
        /* Note : currently supports only the absolute path */
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        if (mofs_strcmp(path, "/") == 0) {
            *inode_num = 2;
        } else {
            path_copy = mofs_malloc(mofs_strlen(path) + 1);
            component = mofs_malloc(mofs_strlen(path) + 1);
            if ((path_copy == NULL) || (component == NULL)) {
                ret = get_errno();
            } else {
                mofs_strcpy(path_copy, path);

                /* find directory entry */
                char *top_path = mofs_strtok(path_copy, "/");
                while (top_path != NULL) {

                    /* fetch top path component */
                    mofs_strcpy(component, top_path);

                    /* find directory entry in the parent directory */
                    ret = find_dir_entry(component, parent_inode_num, &child_inode_num);
                    if ((ret == 0) && (child_inode_num != -1)) {
                        parent_inode_num = child_inode_num;
                    } else {
                        ret = MOFS_ENOENT;
                        break;
                    }
                    top_path = mofs_strtok(NULL, "/");
                }

                if ((ret == 0) && (child_inode_num != -1) && (top_path == NULL)) {
                    *inode_num = parent_inode_num;
                }
            }
        }

        if (path_copy != NULL) {
            mofs_free(path_copy);
        }
        if (component != NULL) {
            mofs_free(component);
        }
    }

    return ret;
}
