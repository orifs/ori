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
 * RWLock Include
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#ifndef __RWLOCK_H__
#define __RWLOCK_H__

#include <stdint.h>

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) \
    || defined(__NetBSD__)
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "UNSUPPORTED OS"
#endif

#include <vector>
#include <map>

#include "thread.h"
#include "mutex.h"

class RWLock;
struct RWKey {
    typedef std::tr1::shared_ptr<RWKey> sp;
    RWKey(RWLock *l = NULL);
    ~RWKey();

    RWLock *lock;
};

class RWLock
{
public:
    RWLock();
    ~RWLock();
    RWKey::sp readLock();
    RWKey::sp tryReadLock();
    RWKey::sp writeLock();
    RWKey::sp tryWriteLock();
    void unlock();
    // bool locked();

    typedef std::vector<uint32_t> LockOrderVector;
    static void setLockOrder(const LockOrderVector &order);
    ///^ lock orderings cannot be changed once set

    uint32_t lockNum;
private:
    static Mutex gOrderMutex;
    static uint32_t gLockNum;
    static std::map<uint32_t, size_t> gLockNumToOrdering;
    static std::vector<LockOrderVector> gLockOrderings;
    static std::map<uint32_t, threadid_t> gLockedBy;

    void _checkLockOrdering();
    void _updateLocked();

#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) \
    || defined(__NetBSD__)
    pthread_rwlock_t lockHandle;
#elif defined(_WIN32)
    SRWLOCK lockHandle;
#else
#error "UNSUPPORTED OS"
#endif
};

#endif /* __RWLOCK_H__ */

