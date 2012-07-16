/*
 * Mutex Include
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __MUTEX_H__
#define __MUTEX_H__

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
#include <pthread.h>
//#elif defined(__WINDOWS__)
//#include <windows.h>
#else
#error "UNSUPPORTED OS"
#endif

class Mutex
{
public:
    Mutex();
    virtual ~Mutex();
    void lock();
    void unlock();
    bool tryLock();
    // bool Locked();
private:
#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
    pthread_mutex_t lockHandle;
//#elif defined(__WINDOWS__)
//    CRITICAL_SECTION lockHandle;
#else
#error "UNSUPPORTED OS"
#endif
};

#endif /* __MUTEX_H__ */

