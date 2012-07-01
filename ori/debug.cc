// TODO: maybe rename this logging.cc?

#define _WITH_DPRINTF

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#include <sys/time.h>

#ifdef __MACH__
#include <mach/mach.h>
#include <mach/clock.h>
#endif

#include "debug.h"
#include "repo.h"

/********************************************************************
 *
 *
 * Logging
 *
 *
 ********************************************************************/

static int logfd = -1;

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
#ifdef DEBUG
        logfd = STDERR_FILENO;
#else
        return;
#endif
    }

    get_timespec(&ts);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&ts.tv_sec));
    dprintf(logfd, "%s", (char *)buf);

    va_start(ap, fmt);
    vdprintf(logfd, fmt, ap);
    va_end(ap);

    // XXX: May cause performance issues disable on release builds
    fsync(logfd);
}

void ori_open_log(Repo *repo) {
    logfd = -1;

    std::string logPath = repo->getLogPath();
    if (logPath == "") return;

    logfd = open(logPath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0660);
    if (logfd == -1) {
        perror("open");
        exit(1);
    }
}
