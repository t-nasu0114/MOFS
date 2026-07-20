#ifndef __MOFS_DEVIO__
#define __MOFS_DEVIO__

#include <mofs_port_types.h>

/**
 * @file mofs_devio.h
 * @brief Contract for byte-addressable backing-device I/O.
 *
 * Every operation returns 0 on success or a positive `MOFS_E*` value on
 * failure. Handles, byte counts, offsets, and capacities are returned through
 * output parameters.
 */

/** Device-open flags. */
/** No device-open flags. */
#define MOFS_IO_OPEN_FLAG_NONE 0
/** Open the backing device for reads. */
#define MOFS_IO_OPEN_FLAG_RDONLY 0b0001
/** Open the backing device for writes. */
#define MOFS_IO_OPEN_FLAG_WRONLY 0b0010
/** Open the backing device for reads and writes. */
#define MOFS_IO_OPEN_FLAG_RDWR (MOFS_IO_OPEN_FLAG_RDONLY | MOFS_IO_OPEN_FLAG_WRONLY)
/** Request synchronous persistence for writes when supported. */
#define MOFS_IO_OPEN_FLAG_SYNC 0b0100
/** Request direct I/O that bypasses platform caches when supported. */
#define MOFS_IO_OPEN_FLAG_DIRECT 0b1000

/** Seek modes. */
/** Seek relative to the start of the device. */
#define MOFS_SEEK_SET 0
/** Seek relative to the current device offset. */
#define MOFS_SEEK_CUR 1
/** Seek relative to the end of the device. */
#define MOFS_SEEK_END 2

/**
 * @brief Open a backing device.
 *
 * @param[in] path Platform-specific device path or device identifier.
 * @param[in] oflag Bitwise combination of `MOFS_IO_OPEN_FLAG_*`.
 * @param[out] fd Destination for the non-negative device handle. Initialized
 *                to -1 before validation or platform I/O.
 * @return 0 on success.
 * @return MOFS_EINVAL when an argument or flag combination is invalid.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_open(const char *path, int oflag, int *fd);

/**
 * @brief Write bytes at the device handle's current offset.
 *
 * @param[in] fd Open device handle.
 * @param[in] buf Buffer containing `count` bytes.
 * @param[in] count Maximum number of bytes to write.
 * @param[out] written Number of bytes written; may be less than `count`.
 * @return 0 on success, including a short write.
 * @return MOFS_EINVAL when an argument is invalid.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_write(int fd, const void *buf, mofs_size_t count, mofs_size_t *written);

/**
 * @brief Read bytes at the device handle's current offset.
 *
 * @param[in] fd Open device handle.
 * @param[out] buf Buffer with capacity for `count` bytes.
 * @param[in] count Maximum number of bytes to read.
 * @param[out] read_size Number of bytes read; zero indicates end of device.
 * @return 0 on success, including a short read or end of device.
 * @return MOFS_EINVAL when an argument is invalid.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_read(int fd, void *buf, mofs_size_t count, mofs_size_t *read_size);

/**
 * @brief Flush pending device writes to stable storage.
 *
 * @param[in] fd Open device handle.
 * @return 0 on success.
 * @return A positive `MOFS_E*` value on platform failure.
 */
int dev_fsync(int fd);

/**
 * @brief Close a backing-device handle.
 *
 * @param[in] fd Open device handle. The handle is invalid after this call.
 * @return 0 on success.
 * @return A positive `MOFS_E*` value on platform failure.
 */
int dev_close(int fd);

/**
 * @brief Reposition a device handle's current byte offset.
 *
 * @param[in] fd Open device handle.
 * @param[in] offset Signed byte displacement.
 * @param[in] whence One of `MOFS_SEEK_SET`, `MOFS_SEEK_CUR`, or
 *                   `MOFS_SEEK_END`.
 * @param[out] new_offset Resulting non-negative byte offset.
 * @return 0 on success.
 * @return MOFS_EINVAL when an argument is invalid.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_lseek(int fd, mofs_off_t offset, int whence, mofs_off_t *new_offset);

/**
 * @brief Obtain the total capacity of a backing device.
 *
 * @param[in] fd Open device handle.
 * @param[out] size Device capacity in bytes.
 * @return 0 on success, including a zero-capacity backing file.
 * @return MOFS_EINVAL when `size` is NULL or the device type is unsupported.
 * @return Another positive `MOFS_E*` value on platform failure.
 */
int dev_get_size(int fd, unsigned long long *size);

#endif /* __MOFS_DEVIO__ */
