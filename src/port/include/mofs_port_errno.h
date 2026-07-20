#ifndef __MOFS_PORT_ERRNO__
#define __MOFS_PORT_ERRNO__

/**
 * @file mofs_port_errno.h
 * @brief Contract for translating platform errors at the OS boundary.
 */

/**
 * @brief Convert a platform-native error number to a MOFS error number.
 *
 * Function behavior:
 * - Maps a positive platform error number to the corresponding `MOFS_E*`.
 * - Returns a generic MOFS I/O error when no exact mapping exists.
 *
 * @param[in] os_errno Platform-native error number; zero means success.
 * @return 0 when `os_errno` is zero.
 * @return A positive `MOFS_E*` value otherwise.
 */
int os_to_mofs_errno(int os_errno);

/**
 * @brief Convert a MOFS error number to a platform-native error number.
 *
 * Function behavior:
 * - Maps a positive `MOFS_E*` value for use by an integration layer.
 * - Returns a generic platform I/O error when no exact mapping exists.
 *
 * @param[in] mofs_errno MOFS error number; zero means success.
 * @return 0 when `mofs_errno` is zero.
 * @return A positive platform-native error number otherwise.
 */
int mofs_to_os_errno(int mofs_errno);

/**
 * @brief Read and translate the calling context's current platform error.
 *
 * Function behavior:
 * - Reads the error state produced by the most recent failing OS service call.
 * - Converts that state through `os_to_mofs_errno()`.
 * - Must preserve per-thread error semantics when the platform provides them.
 *
 * @return 0 when the current platform error state indicates success.
 * @return A positive `MOFS_E*` value otherwise.
 */
int get_errno(void);

#endif /* __MOFS_PORT_ERRNO__ */
