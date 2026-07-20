#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_port_errno.h>
#include <mofs_port_mem.h>
#include <string.h>
#include <zephyr/storage/disk_access.h>

#define MOFS_DEV_NAME_MAX 32
#define MOFS_DEV_SLOT_MAX 16

struct dev_slot
{
    mofs_bool  used;
    char       name[MOFS_DEV_NAME_MAX];
    mofs_off_t offset;
    uint32_t   sector_size;
    uint32_t   sector_count;
    int        open_flags;
};

static struct dev_slot dev_slots[MOFS_DEV_SLOT_MAX];

/**
 * @brief Allocate an unused device-slot index.
 *
 * Function behavior:
 * - Scans `dev_slots` for the first entry with `used == 0`.
 * - Marks that entry used and returns its index.
 *
 * @return Non-negative slot index on success.
 * @return -1 when no free slot remains.
 */
static int dev_slot_alloc(void)
{
    for (int i = 0; i < MOFS_DEV_SLOT_MAX; i++) {
        if (!dev_slots[i].used) {
            dev_slots[i].used = true;
            return i;
        }
    }
    /* No free slot found */
    return -1;
}

/**
 * @brief Release a device slot and clear its contents.
 *
 * Function behavior:
 * - Rejects out-of-range indices.
 * - Zero-fills the slot so a later open starts from a clean state.
 *
 * @param[in] slot Slot index previously returned by `dev_slot_alloc()`.
 * @return 0 on success.
 * @return MOFS_EINVAL when `slot` is out of range.
 */
static int dev_slot_free(int slot)
{
    if (slot < 0 || slot >= MOFS_DEV_SLOT_MAX) {
        return MOFS_EINVAL;
    }
    (void)memset(&dev_slots[slot], 0, sizeof(dev_slots[slot]));
    return 0;
}

/**
 * @brief Resolve an open device handle to its slot.
 *
 * Function behavior:
 * - Accepts only in-range indices that currently have `used != 0`.
 *
 * @param[in] fd Device handle (slot index).
 * @param[out] slot_out Destination for the slot pointer.
 * @return 0 on success.
 * @return MOFS_EBADF when `fd` is invalid or not open.
 */
static int dev_slot_get(int fd, struct dev_slot **slot_out)
{
    if ((fd < 0) || (fd >= MOFS_DEV_SLOT_MAX) || (dev_slots[fd].used == 0)) {
        return MOFS_EBADF;
    }
    *slot_out = &dev_slots[fd];
    return 0;
}

/**
 * @brief Compute the byte capacity of a device slot.
 *
 * Function behavior:
 * - Multiplies `sector_count` by `sector_size` in 64-bit arithmetic.
 *
 * @param[in] slot Open device slot.
 * @return Device size in bytes.
 */
static mofs_off_t dev_slot_byte_size(const struct dev_slot *slot)
{
    return (mofs_off_t)((uint64_t)slot->sector_count * (uint64_t)slot->sector_size);
}

/**
 * @brief Transfer bytes at the slot's current offset via disk_access.
 *
 * Function behavior:
 * - Clamps the request to the remaining device capacity (short I/O is success).
 * - Issues whole-sector `disk_access_read` / `disk_access_write` when aligned.
 * - Uses a temporary sector buffer and read-modify-write for partial sectors.
 * - Advances `slot->offset` by the transferred byte count on success.
 * - Honors `MOFS_IO_OPEN_FLAG_SYNC` with `DISK_IOCTL_CTRL_SYNC` after writes.
 *
 * @param[in,out] slot Open device slot.
 * @param[in,out] buf Destination (read) or source (write) buffer.
 * @param[in] count Requested byte count.
 * @param[out] xfer_out Bytes actually transferred.
 * @param[in] is_write Non-zero to write; zero to read.
 * @return 0 on success, including a short transfer or end of device.
 * @return A positive `MOFS_E*` value on failure.
 */
static int dev_transfer(struct dev_slot *slot, void *buf, mofs_size_t count, mofs_size_t *xfer_out,
                        int is_write)
{
    const uint32_t sector_size = slot->sector_size;
    const mofs_off_t dev_size  = dev_slot_byte_size(slot);
    mofs_off_t       cur       = slot->offset;
    mofs_size_t      remaining;
    mofs_size_t      transferred = 0U;
    uint8_t         *data        = (uint8_t *)buf;
    uint8_t         *sector_buf  = NULL;
    int              err;

    *xfer_out = 0U;

    if (sector_size == 0U) {
        return MOFS_EIO;
    }
    if ((cur >= dev_size) || (count == 0U)) {
        return 0;
    }

    remaining = count;
    if ((mofs_off_t)remaining > (dev_size - cur)) {
        remaining = (mofs_size_t)(dev_size - cur);
    }

    while (remaining > 0U) {
        uint32_t    sector_index = (uint32_t)((uint64_t)cur / (uint64_t)sector_size);
        uint32_t    sector_off   = (uint32_t)((uint64_t)cur % (uint64_t)sector_size);
        mofs_size_t chunk        = (mofs_size_t)sector_size - (mofs_size_t)sector_off;

        if (chunk > remaining) {
            chunk = remaining;
        }

        if ((sector_off == 0U) && (chunk == (mofs_size_t)sector_size)) {
            /* Contiguous full sectors: transfer as many as remain. */
            mofs_size_t nsectors    = remaining / (mofs_size_t)sector_size;
            uint32_t    max_sectors = slot->sector_count - sector_index;

            if (nsectors > (mofs_size_t)max_sectors) {
                nsectors = (mofs_size_t)max_sectors;
            }

            if (is_write != 0) {
                err = disk_access_write(slot->name, data + transferred, sector_index,
                                        (uint32_t)nsectors);
            } else {
                err = disk_access_read(slot->name, data + transferred, sector_index,
                                       (uint32_t)nsectors);
            }
            if (err != 0) {
                if (sector_buf != NULL) {
                    mofs_free(sector_buf);
                }
                return os_to_mofs_errno(-err);
            }
            chunk = nsectors * (mofs_size_t)sector_size;
        } else {
            /* Partial sector: read-modify-write (write) or copy-out (read). */
            if (sector_buf == NULL) {
                sector_buf = (uint8_t *)mofs_malloc((mofs_size_t)sector_size);
                if (sector_buf == NULL) {
                    return MOFS_ENOMEM;
                }
            }

            err = disk_access_read(slot->name, sector_buf, sector_index, 1U);
            if (err != 0) {
                mofs_free(sector_buf);
                return os_to_mofs_errno(-err);
            }

            if (is_write != 0) {
                (void)mofs_memcpy(sector_buf + sector_off, data + transferred, chunk);
                err = disk_access_write(slot->name, sector_buf, sector_index, 1U);
                if (err != 0) {
                    mofs_free(sector_buf);
                    return os_to_mofs_errno(-err);
                }
            } else {
                (void)mofs_memcpy(data + transferred, sector_buf + sector_off, chunk);
            }
        }

        transferred += chunk;
        remaining -= chunk;
        cur += (mofs_off_t)chunk;
    }

    if (sector_buf != NULL) {
        mofs_free(sector_buf);
    }

    slot->offset = cur;
    *xfer_out    = transferred;

    if ((is_write != 0) && ((slot->open_flags & MOFS_IO_OPEN_FLAG_SYNC) != 0)) {
        err = disk_access_ioctl(slot->name, DISK_IOCTL_CTRL_SYNC, NULL);
        if (err != 0) {
            return os_to_mofs_errno(-err);
        }
    }

    return 0;
}

/**
 * @brief Open a disk_access disk and return a MOFS device handle.
 *
 * Function behavior:
 * - Interprets `path` as a Zephyr disk_access disk name (for example `"RAM"`).
 * - Validates open flags and allocates a slot in `dev_slots`.
 * - Issues `DISK_IOCTL_CTRL_INIT`, then refreshes disk status.
 * - Rejects missing media and write-open of a write-protected disk.
 * - Caches sector size/count and resets the byte offset to zero.
 *
 * @param[in] path Disk_access disk name.
 * @param[in] oflag Bitwise combination of `MOFS_IO_OPEN_FLAG_*`.
 * @param[out] fd Destination for the non-negative device handle; set to -1 first.
 * @return 0 on success.
 * @return MOFS_EINVAL when an argument or flag combination is invalid.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_open(const char *path, int oflag, int *fd)
{
    int err         = 0;
    int slot        = -1;
    int status      = 0;
    int known_flags = MOFS_IO_OPEN_FLAG_RDWR | MOFS_IO_OPEN_FLAG_SYNC | MOFS_IO_OPEN_FLAG_DIRECT;

    if (fd == NULL) {
        return MOFS_EINVAL;
    }
    *fd = -1;

    if ((path == NULL) || ((oflag & ~known_flags) != 0) || ((oflag & MOFS_IO_OPEN_FLAG_RDWR) == 0)) {
        return MOFS_EINVAL;
    }

    slot = dev_slot_alloc();
    if (slot < 0) {
        return MOFS_EMFILE;
    }

    /* Reject missing media early. UNINIT is allowed here; CTRL_INIT follows. */
    status = disk_access_status(path);
    if ((status & DISK_STATUS_NOMEDIA) != 0) {
        dev_slot_free(slot);
        return MOFS_ENOENT;
    }

    err = disk_access_ioctl(path, DISK_IOCTL_CTRL_INIT, NULL);
    if (err != 0) {
        dev_slot_free(slot);
        return os_to_mofs_errno(-err);
    }

    /* Re-check status after INIT: media may still be missing or write-protected. */
    status = disk_access_status(path);
    if ((status & DISK_STATUS_NOMEDIA) != 0) {
        dev_slot_free(slot);
        return MOFS_ENOENT;
    }
    if (((status & DISK_STATUS_WR_PROTECT) != 0) && ((oflag & MOFS_IO_OPEN_FLAG_WRONLY) != 0)) {
        dev_slot_free(slot);
        return MOFS_EROFS;
    }

    err = disk_access_ioctl(path, DISK_IOCTL_GET_SECTOR_SIZE, &dev_slots[slot].sector_size);
    if (err != 0) {
        dev_slot_free(slot);
        return os_to_mofs_errno(-err);
    }

    err = disk_access_ioctl(path, DISK_IOCTL_GET_SECTOR_COUNT, &dev_slots[slot].sector_count);
    if (err != 0) {
        dev_slot_free(slot);
        return os_to_mofs_errno(-err);
    }

    strncpy(dev_slots[slot].name, path, MOFS_DEV_NAME_MAX - 1);
    dev_slots[slot].name[MOFS_DEV_NAME_MAX - 1] = '\0';
    dev_slots[slot].open_flags                  = oflag;
    dev_slots[slot].offset                      = 0;
    *fd                                         = slot;

    return 0;
}

/**
 * @brief Write bytes at the device handle's current offset.
 *
 * Function behavior:
 * - Requires the handle to have been opened with write permission.
 * - Delegates the transfer to `dev_transfer()` (sector I/O / RMW).
 *
 * @param[in] fd Open device handle.
 * @param[in] buf Buffer containing `count` bytes.
 * @param[in] count Maximum number of bytes to write.
 * @param[out] written Number of bytes written; may be less than `count`.
 * @return 0 on success, including a short write.
 * @return MOFS_EINVAL when an argument is invalid.
 * @return MOFS_EBADF when `fd` is invalid or not open for writing.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_write(int fd, const void *buf, mofs_size_t count, mofs_size_t *written)
{
    struct dev_slot *slot;
    int              ret;

    if (written == NULL) {
        return MOFS_EINVAL;
    }
    *written = 0U;
    if (buf == NULL) {
        return MOFS_EINVAL;
    }

    ret = dev_slot_get(fd, &slot);
    if (ret != 0) {
        return ret;
    }
    if ((slot->open_flags & MOFS_IO_OPEN_FLAG_WRONLY) == 0) {
        return MOFS_EBADF;
    }

    return dev_transfer(slot, (void *)buf, count, written, 1);
}

/**
 * @brief Read bytes at the device handle's current offset.
 *
 * Function behavior:
 * - Requires the handle to have been opened with read permission.
 * - Delegates the transfer to `dev_transfer()` (sector I/O / partial copy).
 *
 * @param[in] fd Open device handle.
 * @param[out] buf Buffer with capacity for `count` bytes.
 * @param[in] count Maximum number of bytes to read.
 * @param[out] read_size Number of bytes read; zero indicates end of device.
 * @return 0 on success, including a short read or end of device.
 * @return MOFS_EINVAL when an argument is invalid.
 * @return MOFS_EBADF when `fd` is invalid or not open for reading.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_read(int fd, void *buf, mofs_size_t count, mofs_size_t *read_size)
{
    struct dev_slot *slot;
    int              ret;

    if (read_size == NULL) {
        return MOFS_EINVAL;
    }
    *read_size = 0U;
    if (buf == NULL) {
        return MOFS_EINVAL;
    }

    ret = dev_slot_get(fd, &slot);
    if (ret != 0) {
        return ret;
    }
    if ((slot->open_flags & MOFS_IO_OPEN_FLAG_RDONLY) == 0) {
        return MOFS_EBADF;
    }

    return dev_transfer(slot, buf, count, read_size, 0);
}

/**
 * @brief Flush pending device writes through disk_access.
 *
 * Function behavior:
 * - Issues `DISK_IOCTL_CTRL_SYNC` for the disk bound to `fd`.
 *
 * @param[in] fd Open device handle.
 * @return 0 on success.
 * @return MOFS_EBADF when `fd` is invalid or not open.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_fsync(int fd)
{
    struct dev_slot *slot;
    int              ret;
    int              err;

    ret = dev_slot_get(fd, &slot);
    if (ret != 0) {
        return ret;
    }

    err = disk_access_ioctl(slot->name, DISK_IOCTL_CTRL_SYNC, NULL);
    if (err != 0) {
        return os_to_mofs_errno(-err);
    }
    return 0;
}

/**
 * @brief Close a device handle and release its disk_access reference.
 *
 * Function behavior:
 * - Issues `DISK_IOCTL_CTRL_DEINIT` without forcing a stop (balances `CTRL_INIT`).
 * - Releases the MOFS slot even when DEINIT fails so the handle cannot leak.
 *
 * @param[in] fd Open device handle. The handle is invalid after this call.
 * @return 0 on success.
 * @return MOFS_EBADF when `fd` is invalid or not open.
 * @return Another positive `MOFS_E*` value when DEINIT fails.
 */
int dev_close(int fd)
{
    struct dev_slot *slot;
    char             name[MOFS_DEV_NAME_MAX];
    int              ret;
    int              err;

    ret = dev_slot_get(fd, &slot);
    if (ret != 0) {
        return ret;
    }

    /* Keep the disk name; the slot is released even if DEINIT fails. */
    (void)strncpy(name, slot->name, MOFS_DEV_NAME_MAX - 1);
    name[MOFS_DEV_NAME_MAX - 1] = '\0';

    /*
     * Balance DISK_IOCTL_CTRL_INIT from dev_open(). Pass NULL so deinit follows
     * the disk_access reference count (no forced stop).
     */
    err = disk_access_ioctl(name, DISK_IOCTL_CTRL_DEINIT, NULL);

    (void)dev_slot_free(fd);

    if (err != 0) {
        return os_to_mofs_errno(-err);
    }
    return 0;
}

/**
 * @brief Reposition the device handle's current byte offset.
 *
 * Function behavior:
 * - Computes the new offset from `whence` and `offset`.
 * - Rejects negative results and offsets past the device capacity (device-like).
 * - Allows seeking exactly to the end of the device.
 *
 * @param[in] fd Open device handle.
 * @param[in] offset Signed byte displacement.
 * @param[in] whence One of `MOFS_SEEK_SET`, `MOFS_SEEK_CUR`, or `MOFS_SEEK_END`.
 * @param[out] new_offset Resulting non-negative byte offset; set to 0 first.
 * @return 0 on success.
 * @return MOFS_EINVAL when an argument is invalid or the result is out of range.
 * @return MOFS_EBADF when `fd` is invalid or not open.
 */
int dev_lseek(int fd, mofs_off_t offset, int whence, mofs_off_t *new_offset)
{
    struct dev_slot *slot;
    mofs_off_t       new_offset_tmp;
    mofs_off_t       size;
    int              ret;

    if (new_offset == NULL) {
        return MOFS_EINVAL;
    }
    *new_offset = 0;

    ret = dev_slot_get(fd, &slot);
    if (ret != 0) {
        return ret;
    }

    size = dev_slot_byte_size(slot);
    switch (whence) {
    case MOFS_SEEK_SET:
        new_offset_tmp = offset;
        break;
    case MOFS_SEEK_CUR:
        new_offset_tmp = slot->offset + offset;
        break;
    case MOFS_SEEK_END:
        new_offset_tmp = size + offset;
        break;
    default:
        return MOFS_EINVAL;
    }
    if ((new_offset_tmp < 0) || (new_offset_tmp > size)) {
        return MOFS_EINVAL;
    }

    slot->offset  = new_offset_tmp;
    *new_offset   = new_offset_tmp;
    return 0;
}

/**
 * @brief Obtain the total capacity of a backing device.
 *
 * Function behavior:
 * - Returns `sector_count * sector_size` cached at open time.
 *
 * @param[in] fd Open device handle.
 * @param[out] size Device capacity in bytes; set to 0 first.
 * @return 0 on success, including a zero-capacity device.
 * @return MOFS_EINVAL when `size` is NULL.
 * @return MOFS_EBADF when `fd` is invalid or not open.
 */
int dev_get_size(int fd, unsigned long long *size)
{
    struct dev_slot *slot;
    int              ret;

    if (size == NULL) {
        return MOFS_EINVAL;
    }
    *size = 0ULL;

    ret = dev_slot_get(fd, &slot);
    if (ret != 0) {
        return ret;
    }

    *size = (unsigned long long)dev_slot_byte_size(slot);
    return 0;
}
