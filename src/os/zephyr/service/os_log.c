#include <mofs_port_log.h>
#include <stdarg.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h> /* vsnprintk */

LOG_MODULE_REGISTER(mofs, LOG_LEVEL_DBG);

#define MOFS_LOG_BUF_SIZE 128

static void mofs_log_v(uint8_t level, const char *fmt, va_list ap)
{
    char buf[MOFS_LOG_BUF_SIZE];

    (void)vsnprintk(buf, sizeof(buf), fmt, ap);

    switch (level) {
    case LOG_LEVEL_DBG:
        LOG_DBG("%s", buf);
        break;
    case LOG_LEVEL_INF:
        LOG_INF("%s", buf);
        break;
    case LOG_LEVEL_WRN:
        LOG_WRN("%s", buf);
        break;
    default:
        LOG_ERR("%s", buf);
        break;
    }
}

void mofs_log_dbg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_v(LOG_LEVEL_DBG, fmt, ap);
    va_end(ap);
}

void mofs_log_inf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_v(LOG_LEVEL_INF, fmt, ap);
    va_end(ap);
}

void mofs_log_wrn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_v(LOG_LEVEL_WRN, fmt, ap);
    va_end(ap);
}

void mofs_log_err(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_v(LOG_LEVEL_ERR, fmt, ap);
    va_end(ap);
}
