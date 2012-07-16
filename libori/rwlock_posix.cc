/*
 * RWLock Class
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#include "rwlock.h"

RWLock::RWLock()
{
    pthread_rwlock_init(&lockHandle, NULL);
}

RWLock::~RWLock()
{
    pthread_rwlock_destroy(&lockHandle);
}

void RWLock::readLock()
{
    pthread_rwlock_rdlock(&lockHandle);
}

bool RWLock::tryReadLock()
{
    return (pthread_rwlock_tryrdlock(&lockHandle) == 0);
}

void RWLock::writeLock()
{
    pthread_rwlock_wrlock(&lockHandle);
}

bool RWLock::tryWriteLock()
{
    return (pthread_rwlock_trywrlock(&lockHandle) == 0);
}

void RWLock::unlock()
{
    pthread_rwlock_unlock(&lockHandle);
}

