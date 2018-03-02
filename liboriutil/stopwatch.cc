/*
 * Copyright (c) 2012-2013 Stanford University
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
/*
 * System - Stopwatch Class
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdint.h>

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) \
    || defined(__NetBSD__)
#include <sys/types.h>
#include <sys/time.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "UNSUPPORTED OS"
#endif

#include <oriutil/stopwatch.h>

Stopwatch::Stopwatch()
{
    startTime = 0LL;
    elapsedTime = 0LL;
    running = false;
}

Stopwatch::~Stopwatch()
{
}

void
Stopwatch::start()
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    startTime = tv.tv_sec * 1000000 + tv.tv_usec;
    running = true;
}

void
Stopwatch::stop()
{
    uint64_t curTime;
    struct timeval tv;

    gettimeofday(&tv, 0);
    curTime = tv.tv_sec * 1000000 + tv.tv_usec;
    elapsedTime += curTime - startTime;
    running = false;
}

void
Stopwatch::reset()
{
    startTime = 0LL;
    elapsedTime = 0LL;
}

uint64_t
Stopwatch::getElapsedTime()
{
    uint64_t curTime;
    struct timeval tv;

    gettimeofday(&tv, 0);
    curTime = tv.tv_sec * 1000000 + tv.tv_usec;

    if (running == true)
    {
        return elapsedTime + (curTime - startTime);
    } else {
        return elapsedTime;
    }
}

uint64_t
Stopwatch::getElapsedMS()
{
    return getElapsedTime() / 1000;
}

