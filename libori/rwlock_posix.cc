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

RWKey::sp RWLock::readLock()
{
    pthread_rwlock_rdlock(&lockHandle);
    return RWKey::sp(new RWKey(this));
}

bool RWLock::tryReadLock()
{
    return (pthread_rwlock_tryrdlock(&lockHandle) == 0);
}

RWKey::sp RWLock::writeLock()
{
    pthread_rwlock_wrlock(&lockHandle);
    return RWKey::sp(new RWKey(this));
}

bool RWLock::tryWriteLock()
{
    return (pthread_rwlock_trywrlock(&lockHandle) == 0);
}

void RWLock::unlock()
{
    pthread_rwlock_unlock(&lockHandle);
}

