#ifndef __MOFS_PORT_TYPES__
#define __MOFS_PORT_TYPES__

/**
 * @file mofs_port_types.h
 * @brief Entry point for the platform-defined MOFS scalar type contract.
 *
 * Each platform must provide `mofs_os_types.h` under
 * `src/os/<platform>/include/`. The selected header shall define:
 *
 * - Exact-width signed and unsigned types from 8 through 64 bits.
 * - `mofs_size_t` as an unsigned type capable of representing one MOFS I/O
 *   transfer size.
 * - `mofs_off_t` as a signed type capable of representing device offsets.
 * - Filesystem metadata types (`mofs_mode_t`, `mofs_uid_t`, `mofs_gid_t`,
 *   `mofs_pid_t`, `mofs_ino_t`, and `mofs_nlink_t`).
 * - `mofs_bool`, `MOFS_TRUE`, `MOFS_FALSE`, integer limit macros, and `NULL`.
 *
 * On-disk structures depend on the widths of these types. A platform
 * implementation must therefore preserve the widths used by the Linux
 * reference implementation in `src/os/linux/include/mofs_os_types.h`.
 */
#include <mofs_os_types.h>

#endif /* __MOFS_PORT_TYPES__ */
