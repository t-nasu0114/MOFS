#include <mofs_port_log.h>

/* Zephyr stub: discard log output (later: LOG_*). */

void mofs_log_dbg(const char *fmt, ...)
{
    (void)fmt;
}

void mofs_log_inf(const char *fmt, ...)
{
    (void)fmt;
}

void mofs_log_wrn(const char *fmt, ...)
{
    (void)fmt;
}

void mofs_log_err(const char *fmt, ...)
{
    (void)fmt;
}
