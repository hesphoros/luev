#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>
#include "log.h"
#include "sys/__luev_time.h"
#include "event.h"

static void _warn_helper(int severity, int log_errno, const char *fmt,
                         va_list ap);
static void event_log(int severity, const char *msg);

void event_err(int eval, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_ERR, errno, fmt, ap);
    va_end(ap);
    exit(eval);
}

void event_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_WARN, errno, fmt, ap);
    va_end(ap);
}

void event_errx(int eval, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_ERR, -1, fmt, ap);
    va_end(ap);
    exit(eval);
}

void event_warnx(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_WARN, -1, fmt, ap);
    va_end(ap);
}

void event_msgx(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_MSG, -1, fmt, ap);
    va_end(ap);
}

void _event_debugx(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_DEBUG, -1, fmt, ap);
    va_end(ap);
}

static void
_warn_helper(int severity, int log_errno, const char *fmt, va_list ap)
{
    char buf[1024];
    size_t len;

    if (fmt != NULL)
        evutil_vsnprintf(buf, sizeof(buf), fmt, ap);
    else
        buf[0] = '\0';

    if (log_errno >= 0)
    {
        len = strlen(buf);
        if (len < sizeof(buf) - 3)
        {
            evutil_snprintf(buf + len, sizeof(buf) - len, ": %s",
                            strerror(log_errno));
        }
    }

    event_log(severity, buf);
}

static event_log_cb log_fn = NULL;

void event_set_log_callback(event_log_cb cb)
{
    log_fn = cb;
}

static void
event_log(int severity, const char *msg)
{
    if (log_fn)
        log_fn(severity, msg);
    else
    {
        const char *severity_str;
        switch (severity)
        {
        case _EVENT_LOG_DEBUG:
            severity_str = "debug";
            break;
        case _EVENT_LOG_MSG:
            severity_str = "msg";
            break;
        case _EVENT_LOG_WARN:
            severity_str = "warn";
            break;
        case _EVENT_LOG_ERR:
            severity_str = "err";
            break;
        default:
            severity_str = "???";
            break;
        }
        (void)fprintf(stderr, "[%s] %s\n", severity_str, msg);
    }
}
