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

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#endif /* HAVE_EXECINFO */

#include <string>
#include <iostream>
#include <fstream>
#include <stdexcept>

#include <oriutil/debug.h>
#include <oriutil/mutex.h>
#include <oriutil/orifile.h>

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

/*
 * Formats the log entries and append them to the log.  Messages are written to 
 * stderr if they are urgent enough (depending on build).  This function must 
 * not log, throw exceptions, or use our ASSERT, PANIC, NOT_IMPLEMENTED macros.
 */
void
ori_log(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[MAX_LOG];
    size_t off;

#if !defined(DEBUG)
    if (level > LEVEL_MSG)
        return;
#endif /* DEBUG */

#ifndef _WIN32
    struct timespec ts;
    get_timespec(&ts);
    off = strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&ts.tv_sec));
#else
    time_t curTime;
    time(&curTime);
    off = strftime(buf, 32, "%Y-%m-%d %H:%M:%S ", localtime(&curTime));
#endif

    switch (level) {
        case LEVEL_SYS:
            break;
        case LEVEL_ERR:
            strncat(buf, "ERROR: ", MAX_LOG - off);
            break;
        case LEVEL_MSG:
            strncat(buf, "MESSAGE: ", MAX_LOG - off);
            break;
        case LEVEL_LOG:
            strncat(buf, "LOG: ", MAX_LOG - off);
            break;
        case LEVEL_DBG:
            strncat(buf, "DEBUG: ", MAX_LOG - off);
            break;
        case LEVEL_VRB:
            strncat(buf, "VERBOSE: ", MAX_LOG - off);
            break;
    }

    off = strlen(buf);

    va_start(ap, fmt);
    vsnprintf(buf + off, MAX_LOG - off, fmt, ap);
    va_end(ap);

    lock_log.lock();

#ifdef DEBUG
    if (level <= LEVEL_MSG)
        cerr << buf;
#else /* RELEASE or PERF */
    if (level <= LEVEL_ERR)
        cerr << buf + off;
#endif

    if (logStream.is_open()) {
        logStream.write(buf, strlen(buf));

        // Disabled on release builds for performance reasons
#ifdef DEBUG
        logStream.flush();
#endif
    }

    lock_log.unlock();
}

void ori_terminate() {
    static bool thrown = false;
#ifdef HAVE_EXECINFO
    const size_t MAX_FRAMES = 128;
    int num;
    void *array[MAX_FRAMES];
    char **names;
#endif /* HAVE_EXECINFO */

    if (!thrown) {
        try {
            throw;
        } catch (const std::exception &e) {
            ori_log(LEVEL_SYS, "Caught unhandled exception: %s\n", e.what());
        } catch (...) {
            ori_log(LEVEL_SYS, "Caught unhandled exception: (unknown type)");
        }
    }

#ifdef HAVE_EXECINFO
    num = backtrace(array, MAX_FRAMES);
    names = backtrace_symbols(array, num);
    ori_log(LEVEL_SYS, "Backtrace:\n");
    for (int i = 0; i < num; i++) {
        if (names != NULL)
            ori_log(LEVEL_SYS, "[%d] %s\n", i, names[i]);
        else
            ori_log(LEVEL_SYS, "[%d] [0x%p]\n", i, array[i]);
    }
    free(names);
#else
    ori_log(LEVEL_SYS, "Backtrace not support not included in this build\n");
#endif /* HAVE_EXECINFO */
}

int ori_open_log(const string &logPath) {
    if (logPath == "")
        return -1;

    set_terminate(ori_terminate);

    if (!OriFile_Exists(logPath)) {
        OriFile_WriteFile("", logPath);
    }

    logStream.open(logPath.c_str(), fstream::in | fstream::out | fstream::app);
    if (logStream.fail()) {
        printf("Could not open logfile: %s\n", logPath.c_str());
        return -1;
    }

    return 0;
}

/*
 * Pretty print hex data
 */
void
OriDebug_PrintHex(const std::string &data, off_t off, size_t limit)
{
    const size_t row_size = 16;
    bool stop = false;

    //size_t num_printed = 0;
    for (size_t row = 0; !stop; row++) {
        printf("\n");

        printf("%08lx  ", row * row_size);
        for (size_t col = 0; col < row_size; col++) { 
            size_t ix = row * row_size + col;
            if ((limit != 0 && ix >= limit) || ix >= data.length()) {
                stop = true;
                for (; col < row_size; col++) {
                    printf("   ");
                }
                break;
            }
            ix += off;

            printf("%02X ", (unsigned char)data[ix]);
        }
        printf("  |");

        for (size_t col = 0; col < row_size; col++) { 
            size_t ix = row * row_size + col;
            if ((limit != 0 && ix >= limit) || ix >= data.length()) {
                stop = true;
                for (; col < row_size; col++) {
                    printf(" ");
                }
                break;
            }
            ix += off;

            unsigned char c = (unsigned char)data[ix];
            if (c >= 0x20 && c < 0x7F)
                printf("%c", c);
            else
                putchar('.');
        }
        printf("|");
    }
    printf("\n");
}

/*
 * Print a backtrace
 */
void
OriDebug_PrintBacktrace()
{
    const size_t MAX_FRAMES = 128;
    void *array[MAX_FRAMES];

#ifdef HAVE_EXECINFO
    int num = backtrace(array, MAX_FRAMES);
    char **names = backtrace_symbols(array, num);
    for (int i = 0; i < num; i++) {
        fprintf(stderr, "%s\n", names[i]);
    }
    free(names);
#else
    fprintf(stderr, "backtrace not support not included in this build\n");
#endif /* HAVE_EXECINFO */
}

void OriDebug_LogBacktrace()
{
    const size_t MAX_FRAMES = 128;
    void *array[MAX_FRAMES];

#ifdef HAVE_EXECINFO
    int num = backtrace(array, MAX_FRAMES);
    char **names = backtrace_symbols(array, num);
    for (int i = 0; i < num; i++) {
        LOG("%s", names[i]);
    }
    free(names);
#else
    fprintf(stderr, "backtrace not support not included in this build\n");
#endif /* HAVE_EXECINFO */
}

