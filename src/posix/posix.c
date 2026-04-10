/* Note:
    1. APIs in this file updates errno for POSIX compatibility. Therefore, this file should be included after errno.h.
    2. APIs in this file returns error codes defined in POSIX standard.
*/
#include <errno.h>
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_posix.h>

int mofs_fstat(int fd, mofs_stat_t *stbuf)
{
    return 0;
}

/**
 * @brief Open a directory stream in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_opendir_core()` to resolve and open the target directory.
 * - On failure, converts MOFS errno to OS errno and updates `errno`.
 * - Returns the opened directory handle on success.
 *
 * @param[in] path NULL-terminated directory path string.
 * @return Opened directory handle pointer on success.
 * @return NULL on failure (with `errno` updated).
 */
mofs_dirhandle_t *mofs_opendir(const char *path)
{
    mofs_dirhandle_t *handle = NULL;
    int               err    = 0;

    err = mofs_opendir_core(path, &handle);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
    }

    return handle;
}

/**
 * @brief Close a directory stream in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_closedir_core()` to release the opened directory handle.
 * - On failure, converts MOFS errno to OS errno and updates `errno`.
 *
 * @param[in] handle Directory handle to close.
 * @return 0 on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_closedir(mofs_dirhandle_t *handle)
{
    int err = 0;

    err = mofs_closedir_core(&handle);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        err   = -1;
    }

    return err;
}

/**
 * @brief Read the next directory entry in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_readdir_core()` to advance the directory cursor.
 * - Returns a pointer to the per-handle cached entry when a valid entry exists.
 * - Returns NULL at end-of-directory without modifying `errno`.
 * - On failure, returns NULL and updates `errno`.
 *
 * @param[in] handle Opened directory handle.
 * @return Pointer to a valid directory entry on success.
 * @return NULL at end-of-directory or on failure.
 */
mofs_dirent_t *mofs_readdir(mofs_dirhandle_t *handle)
{
    mofs_dirent_t *dirent = NULL;
    int            err    = 0;

    err = mofs_readdir_core(&handle);
    if (err == 0) {
        if ((handle->dirent_buf.inode_num != 0) && (handle->dirent_buf.name[0] != '\0')) {
            dirent = &handle->dirent_buf;
        } else {
            dirent = NULL;
        }
    } else {
        errno = mofs_to_os_errno(err);
    }

    return dirent;
}
