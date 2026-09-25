#include "mofs_vfs.h"

#include <mofs_devio.h>
#include <mofs_lifecycle.h>
#include <mofs_port_errno.h>
#include <mofs_inode.h>
#include <mofs_posix.h>
#include <mofs_types.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/fs_sys.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>

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
static int mofs_vfs_opendir(struct fs_dir_t *dirp, const char *fs_path);
static int mofs_vfs_readdir(struct fs_dir_t *dirp, struct fs_dirent *entry);
static int mofs_vfs_closedir(struct fs_dir_t *dirp);
static int mofs_vfs_unlink(struct fs_mount_t *mountp, const char *name);
static int mofs_vfs_rename(struct fs_mount_t *mountp, const char *from, const char *to);
static int mofs_vfs_mkdir(struct fs_mount_t *mountp, const char *name);
static int mofs_vfs_stat(struct fs_mount_t *mountp, const char *path, struct fs_dirent *entry);
static int mofs_vfs_statvfs(struct fs_mount_t *mountp, const char *path, struct fs_statvfs *stat);
static int mofs_vfs_sync(struct fs_file_t *filp);

static struct fs_file_system_t mofs_fs = {
    .open     = mofs_vfs_open,
    .read     = mofs_vfs_read,
    .write    = mofs_vfs_write,
    .lseek    = mofs_vfs_lseek,
    .tell     = mofs_vfs_tell,
    .truncate = mofs_vfs_truncate,
    .sync     = mofs_vfs_sync,
    .close    = mofs_vfs_close,
    .opendir  = mofs_vfs_opendir,
    .readdir  = mofs_vfs_readdir,
    .closedir = mofs_vfs_closedir,
    .mount    = mofs_mount,
    .unmount  = mofs_unmount,
    .unlink   = mofs_vfs_unlink,
    .rename   = mofs_vfs_rename,
    .mkdir    = mofs_vfs_mkdir,
    .stat     = mofs_vfs_stat,
    .statvfs  = mofs_vfs_statvfs,
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
 * - A missing access mode opens read-only so a missing path is `ENOENT`.
 *   Later read and write still return `EACCES`, because Zephyr flag 0 is not `O_RDONLY`.
 * - Rejects `FS_O_APPEND`. `MOFS_OFLAG_APPEND` is not implemented.
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
        /* Zephyr allows open with no access bits; I/O then returns EACCES. */
        mofs_flags = MOFS_OFLAG_RDONLY;
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
    if ((filp->flags & FS_O_READ) == 0) {
        return -EACCES;
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
    if ((filp->flags & FS_O_WRITE) == 0) {
        return -EACCES;
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
 * @brief Copy a path's final component into a Zephyr directory entry name.
 *
 * @param[out] entry Destination entry.
 * @param[in] path Absolute or stripped path.
 */
static void mofs_vfs_set_basename(struct fs_dirent *entry, const char *path)
{
    const char *base = path;
    const char *slash;

    slash = strrchr(path, '/');
    if ((slash != NULL) && (slash[1] != '\0')) {
        base = slash + 1;
    }
    strncpy(entry->name, base, sizeof(entry->name) - 1U);
    entry->name[sizeof(entry->name) - 1U] = '\0';
}

/**
 * @brief Open a MOFS directory.
 *
 * @param[in,out] dirp Zephyr directory object.
 * @param[in] fs_path Absolute path including the mount point.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_opendir(struct fs_dir_t *dirp, const char *fs_path)
{
    mofs_dirhandle_t *handle;
    const char       *path;

    if ((dirp == NULL) || (dirp->mp == NULL) || (fs_path == NULL)) {
        return -EINVAL;
    }

    path   = mofs_vfs_strip_prefix(fs_path, dirp->mp);
    handle = mofs_opendir(path);
    if (handle == NULL) {
        return mofs_vfs_neg_errno();
    }
    dirp->dirp = handle;
    return 0;
}

/**
 * @brief Read the next MOFS directory entry.
 *
 * Function behavior:
 * - Clears `mofs_errno` before `mofs_readdir()` so end-of-directory stays distinct from errors.
 * - Fills type and size from the entry inode. `.` and `..` are returned as stored.
 *
 * @param[in] dirp Open Zephyr directory object.
 * @param[out] entry Destination directory entry.
 * @return 0 on success, including end of directory (`name[0] == 0`).
 * @return Negative errno on failure.
 */
static int mofs_vfs_readdir(struct fs_dir_t *dirp, struct fs_dirent *entry)
{
    mofs_dirhandle_t *handle;
    mofs_dirent_t    *dirent;
    mofs_inode_t      inode;
    int               err;

    if ((dirp == NULL) || (dirp->dirp == NULL) || (entry == NULL)) {
        return -EINVAL;
    }

    handle     = (mofs_dirhandle_t *)dirp->dirp;
    mofs_errno = 0;
    dirent     = mofs_readdir(handle);
    if (dirent == NULL) {
        if (mofs_errno != 0) {
            return mofs_vfs_neg_errno();
        }
        entry->name[0] = '\0';
        return 0;
    }

    strncpy(entry->name, dirent->name, sizeof(entry->name) - 1U);
    entry->name[sizeof(entry->name) - 1U] = '\0';
    err = mofs_read_inode((int)dirent->inode_num, &inode);
    if (err != 0) {
        return -mofs_to_os_errno(err);
    }
    if ((inode.i_mode & MOFS_FTYPE_DIR) != 0U) {
        entry->type = FS_DIR_ENTRY_DIR;
        entry->size = 0U;
    } else {
        entry->type = FS_DIR_ENTRY_FILE;
        entry->size = (size_t)inode.i_size;
    }
    return 0;
}

/**
 * @brief Close a MOFS directory handle.
 *
 * @param[in,out] dirp Open Zephyr directory object.
 * @return 0 on success.
 * @return Negative errno on failure. `dirp->dirp` is cleared either way.
 */
static int mofs_vfs_closedir(struct fs_dir_t *dirp)
{
    mofs_dirhandle_t *handle;

    if ((dirp == NULL) || (dirp->dirp == NULL)) {
        return -EINVAL;
    }

    handle     = (mofs_dirhandle_t *)dirp->dirp;
    dirp->dirp = NULL;
    if (mofs_closedir(handle) != 0) {
        return mofs_vfs_neg_errno();
    }
    return 0;
}

/**
 * @brief Remove a file, or an empty directory when unlink reports EISDIR.
 *
 * @param[in] mountp Zephyr mount descriptor.
 * @param[in] name Absolute path including the mount point.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_unlink(struct fs_mount_t *mountp, const char *name)
{
    const char *path;

    if ((mountp == NULL) || (name == NULL)) {
        return -EINVAL;
    }

    path = mofs_vfs_strip_prefix(name, mountp);
    if (mofs_unlink(path) == 0) {
        return 0;
    }
    if (mofs_errno == MOFS_EISDIR) {
        if (mofs_rmdir(path) != 0) {
            return mofs_vfs_neg_errno();
        }
        return 0;
    }
    return mofs_vfs_neg_errno();
}

/**
 * @brief Rename a file or directory.
 *
 * @param[in] mountp Zephyr mount descriptor.
 * @param[in] from Existing absolute path.
 * @param[in] to Destination absolute path.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_rename(struct fs_mount_t *mountp, const char *from, const char *to)
{
    const char *old_path;
    const char *new_path;

    if ((mountp == NULL) || (from == NULL) || (to == NULL)) {
        return -EINVAL;
    }

    old_path = mofs_vfs_strip_prefix(from, mountp);
    new_path = mofs_vfs_strip_prefix(to, mountp);
    if (mofs_rename(old_path, new_path) != 0) {
        return mofs_vfs_neg_errno();
    }
    return 0;
}

/**
 * @brief Create a directory with mode 0777.
 *
 * @param[in] mountp Zephyr mount descriptor.
 * @param[in] name Absolute path including the mount point.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_mkdir(struct fs_mount_t *mountp, const char *name)
{
    const char *path;

    if ((mountp == NULL) || (name == NULL)) {
        return -EINVAL;
    }

    path = mofs_vfs_strip_prefix(name, mountp);
    if (mofs_mkdir(path, 0777U) != 0) {
        return mofs_vfs_neg_errno();
    }
    return 0;
}

/**
 * @brief Fill a Zephyr directory entry from `mofs_stat()`.
 *
 * @param[in] mountp Zephyr mount descriptor.
 * @param[in] path Absolute path including the mount point.
 * @param[out] entry Destination entry.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_stat(struct fs_mount_t *mountp, const char *path, struct fs_dirent *entry)
{
    mofs_stat_t st;
    const char *mofs_path;

    if ((mountp == NULL) || (path == NULL) || (entry == NULL)) {
        return -EINVAL;
    }

    mofs_path = mofs_vfs_strip_prefix(path, mountp);
    if (mofs_stat(mofs_path, &st) != 0) {
        return mofs_vfs_neg_errno();
    }

    mofs_vfs_set_basename(entry, mofs_path);
    entry->size = (size_t)st.st_size;
    if ((st.st_mode & MOFS_FTYPE_DIR) != 0U) {
        entry->type = FS_DIR_ENTRY_DIR;
        entry->size = 0U;
    } else {
        entry->type = FS_DIR_ENTRY_FILE;
    }
    return 0;
}

/**
 * @brief Report RAM-disk geometry as volume statistics.
 *
 * Function behavior:
 * - Fills block size and block count from `disk_access`.
 * - Reports every block free. MOFS does not track free space yet.
 *
 * @param[in] mountp Zephyr mount descriptor.
 * @param[in] path Absolute path. Unused.
 * @param[out] stat Destination statistics.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_statvfs(struct fs_mount_t *mountp, const char *path, struct fs_statvfs *stat)
{
    const char *disk;
    uint32_t    sector_count = 0U;
    uint32_t    sector_size  = 0U;

    (void)path;
    if ((mountp == NULL) || (mountp->storage_dev == NULL) || (stat == NULL)) {
        return -EINVAL;
    }

    disk = (const char *)mountp->storage_dev;
    if (disk_access_ioctl(disk, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count) != 0) {
        return -EIO;
    }
    if (disk_access_ioctl(disk, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size) != 0) {
        return -EIO;
    }

    memset(stat, 0, sizeof(*stat));
    stat->f_bsize  = sector_size;
    stat->f_frsize = sector_size;
    stat->f_blocks = sector_count;
    stat->f_bfree  = sector_count;
    return 0;
}

/**
 * @brief Flush the MOFS write-back cache for an open file.
 *
 * @param[in] filp Open Zephyr file object.
 * @return 0 on success.
 * @return Negative errno on failure.
 */
static int mofs_vfs_sync(struct fs_file_t *filp)
{
    mofs_filehandle_t *handle;

    if ((filp == NULL) || (filp->filep == NULL)) {
        return -EINVAL;
    }

    handle = (mofs_filehandle_t *)filp->filep;
    if (mofs_fsync(handle) != 0) {
        return mofs_vfs_neg_errno();
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
