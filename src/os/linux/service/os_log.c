#include <mofs_port_log.h>
#include <stdio.h>
#include <stdarg.h>

#define CLR_RESET  "\033[0m"
#define CLR_RED    "\033[31m"
#define CLR_YELLOW "\033[33m"
#define CLR_CYAN   "\033[36m"

static void mofs_log_vprint(FILE *stream, const char *prefix, const char *color, const char *fmt, va_list ap)
{
    if (color != NULL) {
        (void)fprintf(stream, "%s%s", color, prefix);
    } else {
        (void)fprintf(stream, "%s", prefix);
    }
    (void)vfprintf(stream, fmt, ap);
    if (color != NULL) {
        (void)fprintf(stream, "%s", CLR_RESET);
    }
}

void mofs_log_dbg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_vprint(stdout, "[DBG] ", CLR_CYAN, fmt, ap);
    va_end(ap);
}

void mofs_log_inf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_vprint(stdout, "[INF] ", NULL, fmt, ap);
    va_end(ap);
}

void mofs_log_wrn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_vprint(stderr, "[WRN] ", CLR_YELLOW, fmt, ap);
    va_end(ap);
}

void mofs_log_err(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mofs_log_vprint(stderr, "[ERR] ", CLR_RED, fmt, ap);
    va_end(ap);
}
