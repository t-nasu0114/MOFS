#include "mofs_vfs.h"

#include <mofs_devio.h>
#include <mofs_lifecycle.h>
#include <mofs_port_errno.h>
#include <mofs_posix.h>
#include <mofs_types.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/fs_sys.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mofs_vfs, CONFIG_MOFS_LOG_LEVEL);

static int mofs_mount(struct fs_mount_t *mount);
static int mofs_unmount(struct fs_mount_t *mount);
static int mofs_vfs_open(struct fs_file_t *filp, const char *fs_path, fs_mode_t flags);
static ssize_t mofs_vfs_read(struct fs_file_t *filp, void *dest, size_t nbytes);
static ssize_t mofs_vfs_write(struct fs_file_t *filp, const void *src, size_t nbytes);
static int mofs_vfs_lseek(struct fs_file_t *filp, off_t off, int whence);
static off_t mofs_vfs_tell(struct fs_file_t *filp);
static int mofs_vfs_truncate(struct fs_file_t *filp, off_t length);
static int mofs_vfs_close(struct fs_file_t *filp);

static struct fs_file_system_t mofs_fs = {
    .open     = mofs_vfs_open,
    .read     = mofs_vfs_read,
    .write    = mofs_vfs_write,
    .lseek    = mofs_vfs_lseek,
    .tell     = mofs_vfs_tell,
    .truncate = mofs_vfs_truncate,
    .close    = mofs_vfs_close,
    .mount    = mofs_mount,
    .unmount  = mofs_unmount,
};

/**
 * @brief Drop a Zephyr mount-point prefix from an absolute path.
 *
 * Function behavior:
 * - Advances `path` by `mp->mountp_len`.
 * - Returns `"/"` when the remainder is empty (the mount root).
 *
 * @param[in] path Absolute VFS path beginning with the mount point.
 * @param[in] mp Mount that owns `path`.
 * @return Path inside the MOFS volume.
 */
static const char *mofs_vfs_strip_prefix(const char *path, const struct fs_mount_t *mp)
{
    static const char *const root = "/";

    if ((path == NULL) || (mp == NULL)) {
        return path;
    }

    path += mp->mountp_len;
    return (*path != '\0') ? path : root;
}

/**
 * @brief Translate a failed POSIX-style MOFS call into a negative errno.
 *
 * @return Negative OS errno taken from `mofs_errno`.
 */
static int mofs_vfs_neg_errno(void)
{
    return -mofs_to_os_errno(mofs_errno);
}

/**
 * @brief Open or create a file through MOFS POSIX.
 *
 * Function behavior:
 * - Strips the Zephyr mount-point prefix from `fs_path`.
 * - Maps `FS_O_READ` / `FS_O_WRITE` / `FS_O_CREATE` onto `MOFS_OFLAG_*`.
 * - Rejects `FS_O_APPEND` (`MOFS_OFLAG_APPEND` is not implemented).
 * - Stores the MOFS handle in `filp->filep`.
 *
 * @param[in,out] filp Zephyr file object.
 * @param[in] fs_path Absolute path including the mount point.
 * @param[in] flags Zephyr open flags (`FS_O_*`).
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_open(struct fs_file_t *filp, const char *fs_path, fs_mode_t flags)
{
    int                mofs_flags = 0;
    mofs_filehandle_t *handle     = NULL;
    const char        *path;

    if ((filp == NULL) || (filp->mp == NULL) || (fs_path == NULL)) {
        return -EINVAL;
    }
    if ((flags & FS_O_APPEND) != 0) {
        return -ENOTSUP;
    }
    if ((flags & FS_O_RDWR) == FS_O_RDWR) {
        mofs_flags = MOFS_OFLAG_RDWR;
    } else if ((flags & FS_O_READ) != 0) {
        mofs_flags = MOFS_OFLAG_RDONLY;
    } else if ((flags & FS_O_WRITE) != 0) {
        mofs_flags = MOFS_OFLAG_WRONLY;
    } else {
        return -EINVAL;
    }
    if ((flags & FS_O_CREATE) != 0) {
        mofs_flags |= MOFS_OFLAG_CREAT;
    }

    path   = mofs_vfs_strip_prefix(fs_path, filp->mp);
    handle = mofs_open(path, mofs_flags, 0666U);
    if (handle == NULL) {
        return mofs_vfs_neg_errno();
    }

    filp->filep = handle;
    return 0;
}

/**
 * @brief Read bytes at the MOFS handle's current offset.
 *
 * @param[in] filp Open Zephyr file object.
 * @param[out] dest Destination buffer.
 * @param[in] nbytes Maximum number of bytes to read.
 * @return Number of bytes read on success, including zero at end of file.
 * @return Negative errno on failure.
 */
static ssize_t mofs_vfs_read(struct fs_file_t *filp, void *dest, size_t nbytes)
{
    mofs_filehandle_t *handle;
    int                n;

    if ((filp == NULL) || (filp->filep == NULL) || (dest == NULL)) {
        return -EINVAL;
    }

    handle = (mofs_filehandle_t *)filp->filep;
    n      = mofs_read(handle, dest, (mofs_size_t)nbytes);
    if (n < 0) {
        return mofs_vfs_neg_errno();
    }
    return (ssize_t)n;
}

/**
 * @brief Write bytes at the MOFS handle's current offset.
 *
 * @param[in] filp Open Zephyr file object.
 * @param[in] src Source buffer.
 * @param[in] nbytes Number of bytes to write.
 * @return Number of bytes written on success.
 * @return Negative errno on failure.
 */
static ssize_t mofs_vfs_write(struct fs_file_t *filp, const void *src, size_t nbytes)
{
    mofs_filehandle_t *handle;
    int                n;

    if ((filp == NULL) || (filp->filep == NULL) || (src == NULL)) {
        return -EINVAL;
    }

    handle = (mofs_filehandle_t *)filp->filep;
    n      = mofs_write(handle, src, (mofs_size_t)nbytes);
    if (n < 0) {
        return mofs_vfs_neg_errno();
    }
    return (ssize_t)n;
}

/**
 * @brief Reposition a MOFS file offset.
 *
 * Function behavior:
 * - Delegates to `mofs_lseek()`. `FS_SEEK_*` matches `MOFS_SEEK_*`.
 * - Returns 0 on success. The new position is available from `tell`.
 *
 * @param[in] filp Open Zephyr file object.
 * @param[in] off Signed byte displacement.
 * @param[in] whence `FS_SEEK_SET`, `FS_SEEK_CUR`, or `FS_SEEK_END`.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_lseek(struct fs_file_t *filp, off_t off, int whence)
{
    mofs_filehandle_t *handle;
    mofs_off_t         pos;

    if ((filp == NULL) || (filp->filep == NULL)) {
        return -EINVAL;
    }

    handle = (mofs_filehandle_t *)filp->filep;
    pos    = mofs_lseek(handle, (mofs_off_t)off, whence);
    if (pos < 0) {
        return mofs_vfs_neg_errno();
    }
    return 0;
}

/**
 * @brief Return the current MOFS file offset.
 *
 * Function behavior:
 * - Uses `mofs_lseek(handle, 0, MOFS_SEEK_CUR)`.
 *
 * @param[in] filp Open Zephyr file object.
 * @return Current offset on success.
 * @return Negative errno on failure.
 */
static off_t mofs_vfs_tell(struct fs_file_t *filp)
{
    mofs_filehandle_t *handle;
    mofs_off_t         pos;

    if ((filp == NULL) || (filp->filep == NULL)) {
        return -EINVAL;
    }

    handle = (mofs_filehandle_t *)filp->filep;
    pos    = mofs_lseek(handle, 0, MOFS_SEEK_CUR);
    if (pos < 0) {
        return (off_t)mofs_vfs_neg_errno();
    }
    return (off_t)pos;
}

/**
 * @brief Truncate an open MOFS file.
 *
 * @param[in] filp Open Zephyr file object.
 * @param[in] length New file length in bytes.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_truncate(struct fs_file_t *filp, off_t length)
{
    mofs_filehandle_t *handle;

    if ((filp == NULL) || (filp->filep == NULL) || (length < 0)) {
        return -EINVAL;
    }

    handle = (mofs_filehandle_t *)filp->filep;
    if (mofs_ftruncate(handle, (mofs_off_t)length) != 0) {
        return mofs_vfs_neg_errno();
    }
    return 0;
}

/**
 * @brief Close a MOFS file handle.
 *
 * Function behavior:
 * - Calls `mofs_close()` and clears `filp->filep`.
 *
 * @param[in,out] filp Open Zephyr file object.
 * @return 0 on success.
 * @return Negative errno on failure. `filp->filep` is cleared either way.
 */
static int mofs_vfs_close(struct fs_file_t *filp)
{
    mofs_filehandle_t *handle;
    int                err;

    if ((filp == NULL) || (filp->filep == NULL)) {
        return -EINVAL;
    }

    handle      = (mofs_filehandle_t *)filp->filep;
    err         = mofs_close(handle);
    filp->filep = NULL;
    if (err != 0) {
        return -mofs_to_os_errno(err);
    }
    return 0;
}

/**
 * @brief Mount MOFS through Zephyr VFS.
 *
 * Function behavior:
 * - Treats `mount->storage_dev` as a disk_access disk name string.
 * - Calls `mofs_init_core()` with fixed root uid/gid 0.
 *
 * @param[in] mount Zephyr mount descriptor.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_mount(struct fs_mount_t *mount)
{
    int         err;
    const char *disk;

    if ((mount == NULL) || (mount->storage_dev == NULL)) {
        return -EINVAL;
    }

    disk = (const char *)mount->storage_dev;
    err  = mofs_init_core(disk, MOFS_TRUE, 0U, 0U);
    if (err != 0) {
        return -mofs_to_os_errno(err);
    }
    return 0;
}

/**
 * @brief Unmount MOFS through Zephyr VFS.
 *
 * Function behavior:
 * - Calls `mofs_fini_core()` to release the single global MOFS context.
 *
 * @param[in] mount Zephyr mount descriptor (unused).
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_unmount(struct fs_mount_t *mount)
{
    int err;

    (void)mount;
    err = mofs_fini_core();
    if (err != 0) {
        return -mofs_to_os_errno(err);
    }
    return 0;
}

/**
 * @brief Register MOFS with Zephyr VFS at POST_KERNEL time.
 *
 * @return 0 on success.
 * @return Negative errno from `fs_register()` on failure.
 */
static int mofs_init_zvfs(void)
{
    int err = fs_register(MOFS_FS_TYPE, &mofs_fs);

    if (err != 0) {
        LOG_ERR("Failed to register MOFS filesystem: %d", err);
        return err;
    }
    LOG_INF("MOFS filesystem registered (type=%d)", MOFS_FS_TYPE);
    return 0;
}

SYS_INIT(mofs_init_zvfs, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
