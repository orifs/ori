
#include <cassert>
#include <stdio.h>

#include "rwlock.h"
#include "thread.h"

RWKey::RWKey(RWLock *l)
    : lock(l)
{
}

RWKey::~RWKey()
{
    if (lock)
        lock->unlock();
}

Mutex RWLock::gOrderMutex;
uint32_t RWLock::gLockNum = 0;
std::map<uint32_t, size_t> RWLock::gLockNumToOrdering;
std::vector<RWLock::LockOrderVector> RWLock::gLockOrderings;
std::map<uint32_t, uint64_t> RWLock::gLockedBy;

#define PERMIT_REENTRANT_LOCK 0

void RWLock::setLockOrder(const LockOrderVector &order)
{
    // TODO: error checking (duplicates, etc.)
    gLockOrderings.push_back(order);
    for (size_t i = 0; i < order.size(); i++) {
        gLockNumToOrdering[order[i]] = gLockOrderings.size()-1;
    }
}

void RWLock::_checkLockOrdering() {
    gOrderMutex.lock();

    threadid_t tid = Thread::getID();
    if (PERMIT_REENTRANT_LOCK != 1) {
        if (gLockedBy[lockNum] == tid) {
            fprintf(stderr, "Detected reentrant locking of %u by %u\n",
                    lockNum, tid);
        }
        assert(gLockedBy[lockNum] != tid);
    }

    std::map<uint32_t, size_t>::iterator it = gLockNumToOrdering.find(lockNum);
    if (it != gLockNumToOrdering.end()) {
        const LockOrderVector &order = gLockOrderings[(*it).second];
        bool past = false;
        for (size_t i = 0; i < order.size(); i++) {
            if (order[i] == lockNum) {
                past = true;
                continue;
            }

            if (past) {
                if (gLockedBy[order[i]] == tid) {
                    fprintf(stderr, "Lock %u locked before %u!\n",
                            order[i], lockNum);
                }
                assert(gLockedBy[order[i]] != tid);
            }
        }
    }

    gOrderMutex.unlock();
}

void RWLock::_updateLocked() {
    gOrderMutex.lock();

    threadid_t tid = Thread::getID();
    gLockedBy[lockNum] = tid;

    gOrderMutex.unlock();
}
