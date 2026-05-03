/* fuse_ops.c
 * FUSE operation callbacks for MOFS.
 */

#include "fuse_ops.h"
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <mofs_core.h>
#include <mofs_errno.h>
#include <mofs_inode.h>
#include <mofs_posix.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fuse_operations op = {
    .init    = mofs_init_fuse,
    .destroy = mofs_destroy_fuse,
    .getattr = mofs_getattr_fuse,
    .open    = mofs_open_fuse,
    .release = mofs_release_fuse,
    .readdir = mofs_readdir_fuse,
    .read    = mofs_read_fuse,
};

/**
 * @brief Initialize MOFS core during FUSE startup.
 *
 * Function behavior:
 * - Obtains FUSE private context from `fuse_get_context()`.
 * - Initializes MOFS core using the configured device path.
 * - Terminates the process if initialization fails.
 *
 * @param[in] conn FUSE connection information (unused).
 * @param[in,out] cfg FUSE configuration (currently unused).
 * @param[out] none No output parameters.
 * @return Pointer to FUSE private data on success.
 */
void *mofs_init_fuse(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    int ret = 0;
    (void)conn;
    struct fuse_context *context  = fuse_get_context();
    mofs_fuse_ctx_t     *fuse_ctx = (mofs_fuse_ctx_t *)context->private_data;

    ret = mofs_init_core(fuse_ctx->devfile_path);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize MOFS core: %d\n", ret);
        exit(EXIT_FAILURE);
    }
    return (void *)(context->private_data);
}

/**
 * @brief Finalize MOFS core during FUSE shutdown.
 *
 * Function behavior:
 * - Calls `mofs_fini_core()` to release MOFS resources.
 *
 * @param[in] private_data FUSE private data pointer (unused).
 * @param[out] none No output parameters.
 * @return No return value.
 */
void mofs_destroy_fuse(void *private_data)
{
    (void)private_data;
    mofs_fini_core();
}

/**
 * @brief Retrieve file attributes for a path.
 *
 * Function behavior:
 * - Supports root path lookup (`/` and `/.`).
 * - Resolves inode number and reads inode metadata from MOFS.
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
    (void)fi;
    int         ret = 0;
    mofs_stat_t stat;
    memset(stbuf, 0, sizeof(struct stat));

    ret = mofs_stat_core(path, &stat);
    if (ret == 0) {
        stbuf->st_ino   = stat.st_ino;
        stbuf->st_nlink = stat.st_nlink;
        stbuf->st_size  = stat.st_size;
        stbuf->st_mode  = stat.st_mode;
        stbuf->st_uid   = stat.st_uid;
        stbuf->st_gid   = stat.st_gid;
    }
    return -(mofs_to_os_errno(ret));
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
    int                mofs_flags = 0;
    mode_t             mode       = 0;
    mofs_filehandle_t *handle     = NULL;

    if ((path == NULL) || (fi == NULL)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    switch (fi->flags & O_ACCMODE) {
    case O_RDONLY:
        mofs_flags = MOFS_OFLAG_RDONLY;
        break;
    case O_WRONLY:
        mofs_flags = MOFS_OFLAG_WRONLY;
        break;
    case O_RDWR:
        mofs_flags = MOFS_OFLAG_RDWR;
        break;
    default:
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

#ifdef O_DIRECTORY
    if ((fi->flags & O_DIRECTORY) != 0) {
        mofs_flags |= MOFS_OFLAG_DIRECTORY;
    }
#endif
#ifdef O_CREAT
    if ((fi->flags & O_CREAT) != 0) {
        struct fuse_context *context = fuse_get_context();
        mode = (mode_t)((MOFS_S_IRUSR | MOFS_S_IWUSR | MOFS_S_IRGRP | MOFS_S_IWGRP | MOFS_S_IROTH | MOFS_S_IWOTH) &
                        ~(context->umask));
        mofs_flags |= MOFS_OFLAG_CREAT;
    }
#endif
#if 0 /* Not supported yet */
#ifdef O_EXCL
    if ((fi->flags & O_EXCL) != 0) {
        mofs_flags |= MOFS_OFLAG_EXCL;
    }
#endif
#ifdef O_TRUNC
    if ((fi->flags & O_TRUNC) != 0) {
        mofs_flags |= MOFS_OFLAG_TRUNC;
    }
#endif
#ifdef O_APPEND
    if ((fi->flags & O_APPEND) != 0) {
        mofs_flags |= MOFS_OFLAG_APPEND;
    }
#endif
#ifdef O_SYNC
    if ((fi->flags & O_SYNC) != 0) {
        mofs_flags |= MOFS_OFLAG_SYNC;
    }
#endif
#endif /* Not supported yet */

    handle = mofs_open(path, mofs_flags, mode);
    if (handle == NULL) {
        return -errno;
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

    if ((fi == NULL) || (fi->fh == 0)) {
        return -(mofs_to_os_errno(MOFS_EBADF));
    }

    handle = (mofs_filehandle_t *)(uintptr_t)(fi->fh);
    fi->fh = 0;

    if (mofs_close(handle) != 0) {
        return -errno;
    }

    return 0;
}

/**
 * @brief Read directory entries for a path.
 *
 * Function behavior:
 * - Supports root directory listing (`/` and `/.`).
 * - Adds `.` and `..` entries via FUSE filler callback.
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

    if ((path == NULL) || (buf == NULL) || (filler == NULL) || (offset < 0)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    handle = mofs_opendir(path);
    if (handle == NULL) {
        return -errno;
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
        errno  = 0;
        dirent = mofs_readdir(handle);
        if (dirent == NULL) {
            if (errno != 0) {
                ret = -errno;
            }
            break;
        }

        if (offset > cursor) {
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
        ret = -errno;
    }
    return ret;
}

/**
 * @brief Read file data for a path.
 *
 * Function behavior:
 * - Placeholder implementation. File read operation is not implemented yet.
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
    int                ret    = 0;
    mofs_filehandle_t *handle = NULL;
    (void)path;

    if ((buf == NULL) || (offset < 0) || (fi == NULL) || (fi->fh == 0)) {
        return -(mofs_to_os_errno(MOFS_EINVAL));
    }

    if (size == 0) {
        return 0;
    }

    handle              = (mofs_filehandle_t *)(uintptr_t)(fi->fh);
    handle->file_offset = (unsigned int)offset;
    ret                 = mofs_read(handle, buf, size);
    if (ret < 0) {
        return -errno;
    }

    return ret;
}
