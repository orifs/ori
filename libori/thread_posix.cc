/*
 * Thread Class
 * Copyright (c) 2005-2008 Ali Mashtizadeh
 * All rights reserved.
 */

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>

#include <string>

#include "thread.h"

using namespace std;

// Add support for using Thread::Thread() to get the current thread or the static GetThread().

void EntryWrapper(Thread *t);

Thread::Thread()
{
    cstate = Running;
    tname = "";
    tid = pthread_self(); // How do i fix this for the current thread
}

Thread::Thread(const string &name)
{
    cstate = Running;
    tname = name;
    tid = pthread_self();
}

Thread::~Thread()
{
}

string Thread::getName()
{
    return tname;
}

void Thread::setName(const string &name)
{
    tname = name;
}

ThreadPriority Thread::getPriority()
{
    return NormalPriority;
}

void Thread::setPriority(ThreadPriority p)
{
    //pthread_setschedprio(tid,p);
}

int Thread::terminate()
{
    if (tid == pthread_self())
	pthread_exit(0);
    return pthread_kill(tid, SIGKILL);
}

void Thread::exit(void *retval)
{
    pthread_exit(retval);
}

void Thread::sleep(unsigned long secs)
{
    sleep(secs);
}

void Thread::usleep(useconds_t usecs)
{
    usleep(usecs);
}

void Thread::start()
{
    int ret = pthread_create(&tid,
			     attr,
			     (void *(*)(void *))EntryWrapper,
			     (void *)this);
    //pthread_attr_destroy(attr);
    if (ret != 0) {
	cstate = Running;
	return;
    }
    return;
}

bool Thread::wait(unsigned long time)
{
    // pthread_join
    return false;
}

void Thread::yield()
{
    sched_yield();
}

void EntryWrapper(Thread *t)
{
    t->run();
}

