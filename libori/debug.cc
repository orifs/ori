// TODO: maybe rename this logging.cc?

#define _WITH_DPRINTF

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#include <unistd.h>
#include <sys/time.h>

#ifdef __MACH__
#include <mach/mach.h>
#include <mach/clock.h>
#endif

#include "debug.h"
#include "localrepo.h"
#include "mutex.h"

/********************************************************************
 *
 *
 * Logging
 *
 *
 ********************************************************************/

static int logfd = -1;
static Mutex lock_log;

void
get_timespec(struct timespec *ts)
{
#ifdef __MACH__
    clock_serv_t clk;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clk);
    clock_get_time(clk, &mts);
    mach_port_deallocate(mach_task_self(), clk);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

void
ori_log(const char *fmt, ...)
{
    va_list ap;
    struct timespec ts;
    char buf[32];

    if (logfd == -1) {
        return;
    }

    get_timespec(&ts);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&ts.tv_sec));

    lock_log.lock();
    dprintf(logfd, "%s", (char *)buf);

    va_start(ap, fmt);
    vdprintf(logfd, fmt, ap);
    va_end(ap);

#ifdef DEBUG
    va_start(ap, fmt);
    vdprintf(STDERR_FILENO, fmt, ap);
    va_end(ap);
#endif

    // XXX: May cause performance issues disable on release builds
    fsync(logfd);
    lock_log.unlock();
}

int ori_open_log(LocalRepo *repo) {
    logfd = -1;

    std::string logPath = repo->getLogPath();
    if (logPath == "")
        return -1;

    logfd = open(logPath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0660);
    if (logfd == -1) {
        perror("open");
        printf("couldn't open logfile: %s\n", logPath.c_str());
        return -1;
    }

    return 0;
}
