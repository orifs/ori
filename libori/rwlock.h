/*
 * RWLock Include
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
#include <pthread.h>
//#elif defined(__WINDOWS__)
//#include <windows.h>
#else
#error "UNSUPPORTED OS"
#endif

class RWLock
{
public:
    RWLock();
    ~RWLock();
    void readLock();
    bool tryReadLock();
    void writeLock();
    bool tryWriteLock();
    void unlock();
    // bool locked();
private:
#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
    pthread_rwlock_t lockHandle;
//#elif defined(__WINDOWS__)
//    CRITICAL_SECTION lockHandle;
#else
#error "UNSUPPORTED OS"
#endif
};

#endif /* __RWLOCK_H__ */

