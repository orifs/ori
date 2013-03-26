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

#include <windows.h>

#include <string>
#include <vector>
#include <map>
#include <boost/tr1/memory.hpp>

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
    InitializeSRWLock(&lockHandle);
    lockNum = gLockNum++;
}

RWLock::~RWLock()
{
}

RWKey::sp RWLock::readLock()
{
#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    LOG("%u readLock: %u", tid, lockNum);
    //Util_LogBacktrace();
#endif

#if CHECK_LOCK_ORDER == 1
    _checkLockOrdering();
#endif

    ////////////////////////////
    // Do the actual locking
    AcquireSRWLockShared(&lockHandle);

#if LOG_LOCKING == 1
    LOG("%u success readLock: %u", tid, lockNum);
#endif

#if CHECK_LOCK_ORDER == 1
    _updateLocked();
#endif

    return RWKey::sp(new ReaderKey(this));
}

RWKey::sp RWLock::tryReadLock()
{
    NOT_IMPLEMENTED(false);
    if (TryAcquireSRWLockShared(&lockHandle) != 0) {
        return RWKey::sp(new ReaderKey(this));
    }
    return RWKey::sp();
}

RWKey::sp RWLock::writeLock()
{
#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    LOG("%u writeLock: %u", tid, lockNum);
    //Util_LogBacktrace();
#endif

#if CHECK_LOCK_ORDER == 1
    _checkLockOrdering();
#endif

    ////////////////////////////
    // Do the actual locking
    AcquireSRWLockExclusive(&lockHandle);

#if LOG_LOCKING == 1
    LOG("%u success writeLock: %u", tid, lockNum);
#endif

#if CHECK_LOCK_ORDER == 1
    _updateLocked();
#endif

    return RWKey::sp(new WriterKey(this));
}

RWKey::sp RWLock::tryWriteLock()
{
    NOT_IMPLEMENTED(false);
    if (TryAcquireSRWLockExclusive(&lockHandle) != 0) {
        return RWKey::sp(new WriterKey(this));
    }
    return RWKey::sp();
}

void RWLock::unlock()
{
#if CHECK_LOCK_ORDER == 1
    gOrderMutex.lock();
#endif

    // ReleaseSRWLockShared or ReleaseSRWLockExclusive

#if CHECK_LOCK_ORDER == 1
    gLockedBy[lockNum] = Thread::TID_NOBODY;
    gOrderMutex.unlock();
#endif

#if LOG_LOCKING == 1
    threadid_t tid = Thread::getID();
    LOG("%u unlock: %u", tid, lockNum);
#endif
}

