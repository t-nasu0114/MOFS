/* Note:
    1. APIs in this file updates errno for POSIX compatibility. Therefore, this file should be included after errno.h.
    2. APIs in this file returns error codes defined in POSIX standard.
*/
#include <errno.h>
#include <mofs_core.h>
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_posix.h>

/**
 * @brief Retrieve file status for a path in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_stat_core()` to resolve the path and populate inode metadata.
 * - On failure, converts MOFS errno to OS errno and updates `errno`.
 *
 * @param[in] path NULL-terminated path string.
 * @param[out] stbuf Destination buffer for status fields.
 * @return 0 on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_stat(const char *path, mofs_stat_t *stbuf)
{
    int err = 0;

    err = mofs_stat_core(path, stbuf);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        return -1;
    }

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

/**
 * @brief Open a file handle in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_open_core()` with specified path and open flags.
 * - Converts MOFS error code to OS errno on failure.
 * - Returns an opened file handle on success.
 *
 * @param[in] path NULL-terminated file path string.
 * @param[in] flags Open flags (`MOFS_OFLAG_*`).
 * @param[in] mode File mode used for create path (currently passed through).
 * @return Opened file handle pointer on success.
 * @return NULL on failure (with `errno` updated).
 */
mofs_filehandle_t *mofs_open(const char *path, int flags, mode_t mode)
{
    int                err    = 0;
    mofs_filehandle_t *handle = NULL;

    err = mofs_open_core(path, flags, mode, &handle);
    if (err != 0) {
        errno  = mofs_to_os_errno(err);
        handle = NULL;
    }

    return handle;
}

/**
 * @brief Close an opened file handle in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_close_core()` to release internal file-handle resources.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] handle Opened file handle to close.
 * @return 0 on success.
 * @return Non-zero on failure (with `errno` updated).
 */
int mofs_close(mofs_filehandle_t *handle)
{
    int err = 0;

    err = mofs_close_core(&handle);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
    }

    return err;
}

/**
 * @brief Read file data in POSIX layer.
 *
 * Function behavior:
 * - Validates handle argument before read dispatch.
 * - Uses current `handle->file_offset` as read start offset.
 * - Calls `mofs_read_core()` and requests offset update on success.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] handle Opened file handle.
 * @param[out] buf Destination buffer for read data.
 * @param[in] size Maximum number of bytes to read.
 * @return Number of bytes read on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_read(mofs_filehandle_t *handle, void *buf, size_t size)
{
    if (handle == NULL) {
        errno = EINVAL;
        return -1;
    }
    return mofs_pread(handle, buf, size, (off_t)(handle->file_offset));
}

/**
 * @brief Write file data in POSIX layer.
 *
 * Function behavior:
 * - Validates handle argument before write dispatch.
 * - Uses current `handle->file_offset` as write start offset.
 * - Calls `mofs_write_core()` and requests offset update on success.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] handle Opened file handle.
 * @param[in] buf Source buffer containing bytes to write.
 * @param[in] size Number of bytes requested to write.
 * @return Number of bytes written on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_write(mofs_filehandle_t *handle, const void *buf, size_t size)
{
    if (handle == NULL) {
        errno = EINVAL;
        return -1;
    }
    return mofs_pwrite(handle, buf, size, (off_t)(handle->file_offset));
}

/**
 * @brief Read file data in POSIX layer with explicit offset.
 *
 * Function behavior:
 * - Validates handle and offset arguments before read dispatch.
 * - Calls `mofs_read_core()` and requests offset update on success.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] handle Opened file handle.
 * @param[out] buf Destination buffer for read data.
 * @param[in] size Maximum number of bytes to read.
 * @param[in] offset Read offset in bytes.
 * @return Number of bytes read on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_pread(mofs_filehandle_t *handle, void *buf, size_t size, off_t offset)
{
    int    err          = 0;
    int    ret          = 0;
    off_t  read_offset  = offset;
    off_t  max_offset   = (off_t)UINT32_MAX;
    size_t read_size    = 0;

    if ((handle == NULL) || (offset < 0) || (offset > max_offset)) {
        errno = EINVAL;
        return -1;
    }

    err = mofs_read_core(&handle, buf, size, &read_offset, &read_size, true);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        ret   = -1;
    } else {
        ret = (int)read_size;
    }

    return ret;
}

/**
 * @brief Write file data in POSIX layer with explicit offset.
 *
 * Function behavior:
 * - Validates handle and offset arguments before write dispatch.
 * - Calls `mofs_write_core()` and requests offset update on success.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] handle Opened file handle.
 * @param[in] buf Source buffer containing bytes to write.
 * @param[in] size Number of bytes requested to write.
 * @param[in] offset Write offset in bytes.
 * @return Number of bytes written on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_pwrite(mofs_filehandle_t *handle, const void *buf, size_t size, off_t offset)
{
    int    err          = 0;
    int    ret          = 0;
    off_t  write_offset = offset;
    off_t  max_offset   = (off_t)UINT32_MAX;
    size_t written_size = 0;

    if ((handle == NULL) || (offset < 0) || (offset > max_offset)) {
        errno = EINVAL;
        return -1;
    }

    err = mofs_write_core(&handle, buf, size, &write_offset, &written_size, true);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        ret   = -1;
    } else {
        ret = (int)written_size;
    }

    return ret;
}

/**
 * @brief Unlink a file path in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_unlink_core()` to remove the target file.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] path NULL-terminated file path string.
 * @return 0 on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_unlink(const char *path)
{
    int err = 0;

    err = mofs_unlink_core(path);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        return -1;
    }

    return 0;
}

/**
 * @brief Create a directory path in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_mkdir_core()` to create the target directory.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] path NULL-terminated directory path string.
 * @param[in] mode Permission bits for directory creation.
 * @return 0 on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_mkdir(const char *path, mode_t mode)
{
    int err = 0;

    err = mofs_mkdir_core(path, mode);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        return -1;
    }

    return 0;
}

/**
 * @brief Remove a directory path in POSIX layer.
 *
 * Function behavior:
 * - Calls `mofs_rmdir_core()` to remove the target directory.
 * - Converts MOFS error code to OS errno on failure.
 *
 * @param[in] path NULL-terminated directory path string.
 * @return 0 on success.
 * @return -1 on failure (with `errno` updated).
 */
int mofs_rmdir(const char *path)
{
    int err = 0;

    err = mofs_rmdir_core(path);
    if (err != 0) {
        errno = mofs_to_os_errno(err);
        return -1;
    }

    return 0;
}
