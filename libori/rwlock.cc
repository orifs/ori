#include "rwlock.h"

RWKey::RWKey(RWLock *l)
    : lock(l)
{
}

RWKey::~RWKey()
{
    lock->unlock();
}
