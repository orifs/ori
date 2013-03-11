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
 * Thread Include
 * Copyright (c) 2005-2007 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#include <stdint.h>

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "UNSUPPORTED OS"
#endif

#include <string>

#if defined(__APPLE__)
#include "mach/mach_types.h"
typedef mach_port_t threadid_t;
#elif defined(__linux__)
typedef pid_t threadid_t;
#elif defined(__FreeBSD__)
#include <sys/thr.h>
typedef long threadid_t;
#elif defined(_WIN32)
typedef DWORD threadid_t;
#else
#error "Thread: platform not supported"
#endif


enum ThreadPriority {
    IdlePriority,
    LowPriority,
    NormalPriority,
    HighPriority,
    InheritPriority
};

enum ThreadState { NotStarted, Stopped, Running, Finished };

class Thread
{
public:
    const static threadid_t TID_NOBODY = 0;

    Thread();
    Thread(const std::string &name);
    virtual ~Thread();
    std::string getName();
    void setName(const std::string &name);
    ThreadPriority getPriority();
    void setPriority(ThreadPriority p);
    void start();
    virtual void run() = 0;
    int terminate();
    bool wait(unsigned long time = 0xFFFFFFFF); // ULONG_MAX = wait forever

    static threadid_t getID();
protected:
    static void exit(void *retval);
    void sleep(unsigned long secs);
    void usleep(useconds_t usecs);
    void yield();
private:
    ThreadState cstate;
    std::string tname;
    Thread *entry;
#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
    pthread_t tid;
    pthread_attr_t *attr;
#elif defined(_WIN32)
    HANDLE tid;
#else
#error "UNSUPPORTED OS"
#endif
};

#endif /* __THREAD_H__ */

