/*
 * Mutex Class
 * Copyright (c) 2005 Ali Mashtizadeh
 * All rights reserved.
 */

#include "mutex.h"

Mutex::Mutex()
{
    pthread_mutex_init(&lockHandle, NULL);
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&lockHandle);
}

void Mutex::lock()
{
    pthread_mutex_lock(&lockHandle);
}

void Mutex::unlock()
{
    pthread_mutex_unlock(&lockHandle);
}

bool Mutex::tryLock()
{
    return (pthread_mutex_trylock(&lockHandle) == 0);
}

