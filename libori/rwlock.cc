#include "rwlock.h"

RWKey::RWKey(RWLock *l)
    : lock(l)
{
}

RWKey::~RWKey()
{
    if (lock)
        lock->unlock();
}

uint32_t RWLock::gLockNum = 0;
