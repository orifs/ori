/*
 * Thread Include
 * Copyright (c) 2005-2007 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
#include <limits.h>
#include <pthread.h>
//#elif defined(__WINDOWS__)
//#include <windows.h>
#else
#error "UNSUPPORTED OS"
#endif

#include <string>

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
//#elif defined(__WINDOWS__)
//    HANDLE tid;
#else
#error "UNSUPPORTED OS"
#endif
};

#endif /* __THREAD_H__ */

