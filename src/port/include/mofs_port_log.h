#ifndef __MOFS_PORT_LOG__
#define __MOFS_PORT_LOG__

/**
 * @file mofs_port_log.h
 * @brief Contract for platform-routed MOFS diagnostic logging.
 *
 * Implementations route messages to the platform logging backend. Logging
 * functions do not report backend failures and must not alter MOFS control
 * flow.
 */

/**
 * @brief Emit a debug-level formatted message.
 *
 * @param[in] fmt Format string followed by arguments required by the format.
 */
void mofs_log_dbg(const char *fmt, ...);

/**
 * @brief Emit an informational formatted message.
 *
 * @param[in] fmt Format string followed by arguments required by the format.
 */
void mofs_log_inf(const char *fmt, ...);

/**
 * @brief Emit a warning-level formatted message.
 *
 * @param[in] fmt Format string followed by arguments required by the format.
 */
void mofs_log_wrn(const char *fmt, ...);

/**
 * @brief Emit an error-level formatted message.
 *
 * @param[in] fmt Format string followed by arguments required by the format.
 */
void mofs_log_err(const char *fmt, ...);

#endif /* __MOFS_PORT_LOG__ */
