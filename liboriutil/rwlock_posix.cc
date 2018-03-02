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
 * RWLock Class
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>

#include <string>
#include <memory>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/rwlock.h>
#include <oriutil/thread.h>

#define LOG_LOCKING 0

#ifdef DEBUG
#define CHECK_LOCK_ORDER 1
#else
#define CHECK_LOCK_ORDER 0
#endif

RWLock::RWLock()
    : lockHandle()
{
    pthread_rwlock_init(&lockHandle, NULL);
    lockNum = gLockNum++;
}

RWLock::~RWLock()
{
    pthread_rwlock_destroy(&lockHandle);
}

RWKey::sp RWLock::readLock()
{
#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    DLOG("%u readLock: %u", tid, lockNum);
#endif

#if CHECK_LOCK_ORDER == 1
    _checkLockOrdering();
#endif

    ////////////////////////////
    // Do the actual locking
    pthread_rwlock_rdlock(&lockHandle);

#if LOG_LOCKING == 1
    DLOG("%u success readLock: %u", tid, lockNum);
#endif

#if CHECK_LOCK_ORDER == 1
    _updateLocked();
#endif

    return RWKey::sp(new ReaderKey(this));
}

RWKey::sp RWLock::tryReadLock()
{
    NOT_IMPLEMENTED(false);
    if (pthread_rwlock_tryrdlock(&lockHandle) == 0) {
        return RWKey::sp(new ReaderKey(this));
    }
    return RWKey::sp();
}

void RWLock::readUnlock()
{
#if CHECK_LOCK_ORDER == 1
    gOrderMutex.lock();
#endif

    pthread_rwlock_unlock(&lockHandle);
    
#if CHECK_LOCK_ORDER == 1
    gLockedBy[lockNum] = Thread::TID_NOBODY;
    gOrderMutex.unlock();
#endif

#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    DLOG("%u readUnlock: %u", tid, lockNum);
#endif
}

RWKey::sp RWLock::writeLock()
{
#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    DLOG("%u writeLock: %u", tid, lockNum);
#endif

#if CHECK_LOCK_ORDER == 1
    _checkLockOrdering();
#endif

    ////////////////////////////
    // Do the actual locking
    pthread_rwlock_wrlock(&lockHandle);

#if LOG_LOCKING == 1
    DLOG("%u success writeLock: %u", tid, lockNum);
#endif

#if CHECK_LOCK_ORDER == 1
    _updateLocked();
#endif

    return RWKey::sp(new WriterKey(this));
}

RWKey::sp RWLock::tryWriteLock()
{
    NOT_IMPLEMENTED(false);
    if (pthread_rwlock_trywrlock(&lockHandle) == 0) {
        return RWKey::sp(new WriterKey(this));
    }
    return RWKey::sp();
}

void RWLock::writeUnlock()
{
#if CHECK_LOCK_ORDER == 1
    gOrderMutex.lock();
#endif

    pthread_rwlock_unlock(&lockHandle);
    
#if CHECK_LOCK_ORDER == 1
    gLockedBy[lockNum] = Thread::TID_NOBODY;
    gOrderMutex.unlock();
#endif

#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    DLOG("%u writeUnlock: %u", tid, lockNum);
#endif
}

