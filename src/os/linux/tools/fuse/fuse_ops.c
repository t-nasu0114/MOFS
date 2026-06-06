/* fuse_ops.c
 * FUSE operation callbacks for MOFS.
 */

#include "fuse_ops.h"
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <mofs_errno.h>
#include <mofs_lifecycle.h>
#include <mofs_posix.h>
#include <mofs_user.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void mofs_fuse_bind_request_caller(void)
{
    struct fuse_context *ctx = fuse_get_context();

    if (ctx != NULL) {
        (void)mofs_set_caller_for_peer_process(ctx->uid, ctx->gid, ctx->pid);
    }
}

static int fuse_neg_errno_from_mofs(void)
{
    return -(mofs_to_os_errno(mofs_errno));
}

struct fuse_operations op = {
    .init    = mofs_init_fuse,
    .destroy = mofs_destroy_fuse,
    .getattr = mofs_getattr_fuse,
    .truncate = mofs_truncate_fuse,
    .mkdir   = mofs_mkdir_fuse,
    .rmdir   = mofs_rmdir_fuse,
    .unlink  = mofs_unlink_fuse,
    .create  = mofs_create_fuse,
    .open    = mofs_open_fuse,
    .release = mofs_release_fuse,
    .readdir = mofs_readdir_fuse,
    .read    = mofs_read_fuse,
    .write   = mofs_write_fuse,
};

static int build_mofs_open_args(int fuse_flags, mode_t requested_mode, bool force_create, int *mofs_flags, mode_t *mode)
{
    int                  flags = 0;
    mode_t               out_mode;
    struct fuse_context *context = NULL;

    if ((mofs_flags == NULL) || (mode == NULL)) {
        return MOFS_EINVAL;
    }

#if defined(O_PATH)
    if ((fuse_flags & O_PATH) != 0) {
        if (((fuse_flags & O_ACCMODE) != 0) && ((fuse_flags & O_ACCMODE) != O_RDONLY)) {
            return MOFS_EINVAL;
        }
        flags = MOFS_OFLAG_SEARCH;
    } else
#endif
    {
        switch (fuse_flags & O_ACCMODE) {
        case O_RDONLY:
            flags = MOFS_OFLAG_RDONLY;
            break;
        case O_WRONLY:
            flags = MOFS_OFLAG_WRONLY;
            break;
        case O_RDWR:
            flags = MOFS_OFLAG_RDWR;
            break;
        default:
            return MOFS_EINVAL;
        }
    }

#ifdef O_DIRECTORY
    if ((fuse_flags & O_DIRECTORY) != 0) {
        flags |= MOFS_OFLAG_DIRECTORY;
    }
#endif
#ifdef O_EXCL
    if ((fuse_flags & O_EXCL) != 0) {
        flags |= MOFS_OFLAG_EXCL;
    }
#endif
#if 0 /* Not supported */
#ifdef O_TRUNC
    if ((fuse_flags & O_TRUNC) != 0) {
        flags |= MOFS_OFLAG_TRUNC;
    }
#endif
#ifdef O_APPEND
    if ((fuse_flags & O_APPEND) != 0) {
        flags |= MOFS_OFLAG_APPEND;
    }
#endif
#ifdef O_SYNC
    if ((fuse_flags & O_SYNC) != 0) {
        flags |= MOFS_OFLAG_SYNC;
    }
#endif
#endif /* Not supported */
    out_mode = requested_mode;
#ifdef O_CREAT
    if (force_create || ((fuse_flags & O_CREAT) != 0)) {
        flags |= MOFS_OFLAG_CREAT;
        context = fuse_get_context();
        if (out_mode == 0U) {
            out_mode = (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        }
        if (context != NULL) {
            out_mode = (mode_t)(out_mode & ~(context->umask));
        }
    }
#endif

    *mofs_flags = flags;
    *mode       = out_mode;
    return 0;
}

/**
 * @brief Initialize MOFS core during FUSE startup.
 *
 * Function behavior:
 * - Obtains FUSE private context from `fuse_get_context()`.
 * - Initializes MOFS core using the configured device path.
 * - Updates root inode uid/gid to the effective uid/gid of the `mofs` process
 *   (`geteuid` / `getegid`), since `fuse_get_context()` is not reliable in `init`.
 * - Binds MOFS caller credentials for subsequent operations in this thread.
 * - Terminates the process if initialization fails.
 *
 * @param[in] conn FUSE connection information (unused).
 * @param[in,out] cfg FUSE configuration (currently unused).
 * @param[out] none No output parameters.
 * @return Pointer to FUSE private data on success.
 */
void *mofs_init_fuse(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    int                  ret      = 0;
    struct fuse_context *context  = NULL;
    mofs_fuse_ctx_t     *fuse_ctx = NULL;

    (void)conn;
    (void)cfg;

    context = fuse_get_context();
    if (context == NULL) {
        fprintf(stderr, "FUSE context unavailable\n");
        exit(EXIT_FAILURE);
    }

    fuse_ctx = (mofs_fuse_ctx_t *)context->private_data;

    ret = mofs_init_core(fuse_ctx->devfile_path, true, (uint32_t)geteuid(), (uint32_t)getegid());
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize MOFS core: %d\n", ret);
        exit(EXIT_FAILURE);
    }

    (void)mofs_set_caller_for_peer_process(geteuid(), getegid(), getpid());

    return (void *)(fuse_ctx);
}

/**
 * @brief Finalize MOFS core during FUSE shutdown.
 *
 * Function behavior:
 * - Clears per-thread caller credentials used for permission checks.
 * - Calls `mofs_fini_core()` to release MOFS resources.
 *
 * @param[in] private_data FUSE private data pointer (unused).
 * @param[out] none No output parameters.
 * @return No return value.
 */
void mofs_destroy_fuse(void *private_data)
{
    (void)private_data;
    (void)mofs_clear_caller_user();
    mofs_fini_core();
}

/**
 * @brief Retrieve file attributes for a path.
 *
 * Function behavior:
 * - Supports root path lookup (`/` and `/.`).
 * - Obtains inode metadata via `mofs_stat()` (POSIX layer).
 * - Fills `struct stat` and converts MOFS errors to OS errno values.
 *
 * @param[in] path Target path string.
 * @param[out] stbuf Output `stat` structure to populate.
 * @param[in] fi FUSE file info (unused).
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_getattr_fuse(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    int         ret = 0;
    mofs_stat_t stat;

    (void)fi;
    mofs_fuse_bind_request_caller();
    memset(stbuf, 0, sizeof(struct stat));

    ret = mofs_stat(path, &stat);
    if (ret == 0) {
        stbuf->st_ino   = stat.st_ino;
        stbuf->st_nlink = stat.st_nlink;
        stbuf->st_size  = stat.st_size;
        stbuf->st_mode  = stat.st_mode;
        stbuf->st_uid   = stat.st_uid;
        stbuf->st_gid   = stat.st_gid;
        stbuf->st_atime = stat.st_atime_sec;
        stbuf->st_mtime = stat.st_mtime_sec;
        stbuf->st_ctime = stat.st_ctime_sec;
        return 0;
    }
    return fuse_neg_errno_from_mofs();
}

/**
 * @brief Change the size of a file for a path.
 *
 * Function behavior:
 * - Calls `mofs_truncate()` or `mofs_ftruncate()` depending on open handle availability.
 * - Uses an open file handle from `fi->fh` when available.
 *
 * @param[in] path Target path string.
 * @param[in] size New file size in bytes.
 * @param[in] fi FUSE file info that may hold an open handle.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_truncate_fuse(const char *path, off_t size, struct fuse_file_info *fi)
{
    mofs_fuse_bind_request_caller();

    if (path == NULL) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    /* Prefer ftruncate when FUSE passes an open handle (e.g. ftruncate(2) on open fd). */
    if ((fi != NULL) && (fi->fh != 0U)) {
        if (mofs_ftruncate((mofs_filehandle_t *)(uintptr_t)fi->fh, size) != 0) {
            return fuse_neg_errno_from_mofs();
        }
    } else if (mofs_truncate(path, size) != 0) {
        return fuse_neg_errno_from_mofs();
    }

    return 0;
}

/**
 * @brief Open a file and cache handle in FUSE file info.
 *
 * Function behavior:
 * - Validates input arguments.
 * - Converts FUSE/Linux open flags to MOFS open flags.
 * - Opens the target path through POSIX wrapper and stores handle in `fi->fh`.
 *
 * @param[in] path Target file path.
 * @param[in,out] fi FUSE file info that stores per-open handle.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_open_fuse(const char *path, struct fuse_file_info *fi)
{
    int                ret        = 0;
    int                mofs_flags = 0;
    mode_t             mode       = 0;
    mofs_filehandle_t *handle     = NULL;

    mofs_fuse_bind_request_caller();

    if ((path == NULL) || (fi == NULL)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    ret = build_mofs_open_args(fi->flags, 0U, false, &mofs_flags, &mode);
    if (ret != 0) {
        return -(mofs_to_os_errno(ret));
    }

    handle = mofs_open(path, mofs_flags, mode);
    if (handle == NULL) {
        return fuse_neg_errno_from_mofs();
    }

    fi->fh = (uint64_t)(uintptr_t)handle;
    return 0;
}

/**
 * @brief Create a file and cache handle in FUSE file info.
 *
 * Function behavior:
 * - Converts FUSE/Linux open flags and create mode to MOFS arguments.
 * - Opens path through POSIX wrapper with create flag and stores handle in `fi->fh`.
 *
 * @param[in] path Target file path.
 * @param[in] mode Create mode bits provided by FUSE.
 * @param[in,out] fi FUSE file info that stores per-open handle.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_create_fuse(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int                ret        = 0;
    int                mofs_flags = 0;
    mode_t             mofs_mode  = 0;
    mofs_filehandle_t *handle     = NULL;

    mofs_fuse_bind_request_caller();

    if ((path == NULL) || (fi == NULL)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    ret = build_mofs_open_args(fi->flags, mode, true, &mofs_flags, &mofs_mode);
    if (ret != 0) {
        return -(mofs_to_os_errno(ret));
    }

    handle = mofs_open(path, mofs_flags, mofs_mode);
    if (handle == NULL) {
        return fuse_neg_errno_from_mofs();
    }

    fi->fh = (uint64_t)(uintptr_t)handle;
    return 0;
}

/**
 * @brief Release a file handle stored in FUSE file info.
 *
 * Function behavior:
 * - Retrieves per-open file handle from `fi->fh`.
 * - Closes the handle using POSIX wrapper.
 * - Clears `fi->fh` after release.
 *
 * @param[in] path Target file path (unused).
 * @param[in,out] fi FUSE file info that owns the handle.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_release_fuse(const char *path, struct fuse_file_info *fi)
{
    mofs_filehandle_t *handle = NULL;

    (void)path;

    mofs_fuse_bind_request_caller();

    if ((fi == NULL) || (fi->fh == 0)) {
        return -(mofs_to_os_errno(MOFS_EBADF));
    }

    handle = (mofs_filehandle_t *)(uintptr_t)(fi->fh);
    fi->fh = 0;

    if (mofs_close(handle) != 0) {
        return fuse_neg_errno_from_mofs();
    }

    return 0;
}

/**
 * @brief Unlink a file path through FUSE.
 *
 * Function behavior:
 * - Calls POSIX wrapper `mofs_unlink()`.
 * - Returns negative `errno` on failure.
 *
 * @param[in] path Target file path.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_unlink_fuse(const char *path)
{
    mofs_fuse_bind_request_caller();

    if (path == NULL) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }
    if (mofs_unlink(path) != 0) {
        return fuse_neg_errno_from_mofs();
    }
    return 0;
}

/**
 * @brief Create a directory path through FUSE.
 *
 * Function behavior:
 * - Applies caller umask to requested mode.
 * - Calls POSIX wrapper `mofs_mkdir()`.
 * - Returns negative `errno` on failure.
 *
 * @param[in] path Target directory path.
 * @param[in] mode Requested directory mode.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_mkdir_fuse(const char *path, mode_t mode)
{
    mode_t               masked_mode = mode;
    struct fuse_context *context     = NULL;

    mofs_fuse_bind_request_caller();

    if (path == NULL) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    context = fuse_get_context();
    if (context != NULL) {
        masked_mode = (mode_t)(masked_mode & ~(context->umask));
    }

    if (mofs_mkdir(path, masked_mode) != 0) {
        return fuse_neg_errno_from_mofs();
    }
    return 0;
}

/**
 * @brief Remove an empty directory path through FUSE.
 *
 * Function behavior:
 * - Calls POSIX wrapper `mofs_rmdir()`.
 * - Returns negative `errno` on failure.
 *
 * @param[in] path Target directory path.
 * @return 0 on success.
 * @return Negative errno value on failure.
 */
int mofs_rmdir_fuse(const char *path)
{
    mofs_fuse_bind_request_caller();

    if (path == NULL) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }
    if (mofs_rmdir(path) != 0) {
        return fuse_neg_errno_from_mofs();
    }
    return 0;
}

/**
 * @brief Read directory entries for a path.
 *
 * Function behavior:
 * - Supports root directory listing (`/` and `/.`).
 * - Adds `.` and `..` via FUSE filler (mkdir also stores them on disk; skip duplicates).
 *
 * @param[in] path Target directory path.
 * @param[out] buf FUSE directory buffer passed to filler callback.
 * @param[in] filler Callback used to append directory entries.
 * @param[in] offset Directory read offset (unused).
 * @param[in] fi FUSE file info (unused).
 * @param[in] flags FUSE readdir flags (unused).
 * @return 0 on success.
 * @return -ENOENT if the path is not supported.
 */
int mofs_readdir_fuse(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    int               ret    = 0;
    off_t             cursor = 0;
    mofs_dirhandle_t *handle = NULL;
    mofs_dirent_t    *dirent = NULL;

    (void)flags;
    (void)fi;

    mofs_fuse_bind_request_caller();

    if ((path == NULL) || (buf == NULL) || (filler == NULL) || (offset < 0)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    handle = mofs_opendir(path);
    if (handle == NULL) {
        return fuse_neg_errno_from_mofs();
    }

    /* offset is a readdir cookie, not byte offset. */
    if (offset <= cursor) {
        cursor++;
        if (filler(buf, ".", NULL, cursor, 0) != 0) {
            goto out;
        }
    } else {
        cursor++;
    }

    if (offset <= cursor) {
        cursor++;
        if (filler(buf, "..", NULL, cursor, 0) != 0) {
            goto out;
        }
    } else {
        cursor++;
    }

    while (true) {
        mofs_errno = 0;
        dirent = mofs_readdir(handle);
        if (dirent == NULL) {
            if (mofs_errno != 0) {
                ret = fuse_neg_errno_from_mofs();
            }
            break;
        }

        if (offset > cursor) {
            cursor++;
            continue;
        }

        if ((strcmp(dirent->name, ".") == 0) || (strcmp(dirent->name, "..") == 0)) {
            cursor++;
            continue;
        }

        cursor++;
        if (filler(buf, dirent->name, NULL, cursor, 0) != 0) {
            break;
        }
    }

out:
    if ((mofs_closedir(handle) != 0) && (ret == 0)) {
        ret = fuse_neg_errno_from_mofs();
    }
    return ret;
}

/**
 * @brief Read file data for a path.
 *
 * Function behavior:
 * - Uses opened handle from `fi->fh` and dispatches to POSIX wrapper `mofs_read()`.
 * - Synchronizes file offset with requested FUSE read offset.
 *
 * @param[in] path Target file path.
 * @param[out] buf Destination buffer for read data.
 * @param[in] size Number of bytes requested.
 * @param[in] offset Read offset in bytes.
 * @param[in] fi FUSE file info.
 * @return Negative OS errno for `MOFS_ENOSYS` because this operation is not
 *         implemented.
 */
int mofs_read_fuse(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;

    (void)path;

    mofs_fuse_bind_request_caller();

    if ((buf == NULL) || (offset < 0) || ((uint64_t)offset > (uint64_t)UINT32_MAX) || (fi == NULL) || (fi->fh == 0)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    if (size == 0) {
        return 0;
    }

    ret = mofs_pread((mofs_filehandle_t *)(uintptr_t)(fi->fh), buf, size, offset);
    if (ret < 0) {
        return fuse_neg_errno_from_mofs();
    }

    return ret;
}

/**
 * @brief Write file data for a path.
 *
 * Function behavior:
 * - Uses opened handle from `fi->fh` and dispatches to POSIX wrapper `mofs_write()`.
 * - Synchronizes file offset with requested FUSE write offset.
 *
 * @param[in] path Target file path.
 * @param[in] buf Source buffer containing write data.
 * @param[in] size Number of bytes requested to write.
 * @param[in] offset Write offset in bytes.
 * @param[in] fi FUSE file info.
 * @return Number of bytes written on success.
 * @return Negative errno value on failure.
 */
int mofs_write_fuse(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int ret = 0;

    (void)path;

    mofs_fuse_bind_request_caller();

    if ((buf == NULL) || (offset < 0) || ((uint64_t)offset > (uint64_t)UINT32_MAX) || (fi == NULL) || (fi->fh == 0)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    if (size == 0U) {
        return 0;
    }

    ret = mofs_pwrite((mofs_filehandle_t *)(uintptr_t)(fi->fh), buf, size, offset);
    if (ret < 0) {
        return fuse_neg_errno_from_mofs();
    }

    return ret;
}
