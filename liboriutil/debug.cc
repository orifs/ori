/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#endif

#ifdef __MACH__
#include <mach/mach.h>
#include <mach/clock.h>
#endif

#include <string>
#include <iostream>
#include <fstream>

#include <oriutil/debug.h>
#include <oriutil/mutex.h>

using namespace std;

/********************************************************************
 *
 *
 * Logging
 *
 *
 ********************************************************************/

static fstream logStream;
static Mutex lock_log;

#ifndef _WIN32
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
#endif

#define MAX_LOG         512

void
ori_log(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[MAX_LOG];

#if !defined(DEBUG)
    if (level > LEVEL_MSG)
        return;
#endif /* DEBUG */

#ifndef _WIN32
    struct timespec ts;
    get_timespec(&ts);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&ts.tv_sec));
#else
    time_t curTime;
    time(&curTime);
    strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&curTime));
#endif

    switch (level) {
        case LEVEL_ERR:
            strncat(buf, "ERROR: ", MAX_LOG);
            break;
        case LEVEL_MSG:
            strncat(buf, "MESSAGE: ", MAX_LOG);
            break;
        case LEVEL_LOG:
            strncat(buf, "LOG: ", MAX_LOG);
            break;
        case LEVEL_DBG:
            strncat(buf, "DEBUG: ", MAX_LOG);
            break;
        case LEVEL_VRB:
            strncat(buf, "VERBOSE: ", MAX_LOG);
            break;
    }

    size_t off = strlen(buf);

    va_start(ap, fmt);
    vsnprintf(buf + off, MAX_LOG - off, fmt, ap);
    va_end(ap);

    lock_log.lock();

#ifdef DEBUG
    if (level <= LEVEL_MSG)
        cerr << buf;
#else /* RELEASE or PERF */
    if (level <= LEVEL_ERR)
        cerr << buf;
#endif

    if (logStream.is_open())
        logStream.write(buf, strlen(buf));

    // XXX: May cause performance issues disable on release builds
#ifdef DEBUG
    logStream.flush();
#endif

    lock_log.unlock();
}

int ori_open_log(const string &logPath) {
    if (logPath == "")
        return -1;

    logStream.open(logPath, fstream::in | fstream::out | fstream::app);
    if (logStream.fail()) {
        printf("Could not open logfile: %s\n", logPath.c_str());
        return -1;
    }

    return 0;
}
