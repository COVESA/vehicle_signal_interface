/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file sharedMemoryLocks.h

    This file contains the declarations of the semaphore objects and member
    functions.

-----------------------------------------------------------------------------*/

#ifndef SHARED_MEMORY_LOCKS_H
#define SHARED_MEMORY_LOCKS_H

#include <pthread.h>


/*! @{ */

//
//  Define the semaphore control structure.
//
typedef struct semaphore_t
{
    pthread_mutex_t mutex;
    pthread_cond_t  conditionVariable;
    int             messageCount;
    int             waiterCount;

}   semaphore_t, *semaphore_p;


//
//  Define the semaphore member functions.
//
void semaphorePost ( semaphore_p semaphore );
void semaphoreWait ( semaphore_p semaphore );


#ifdef SEM_DUMP
    extern void dumpSemaphore ( semaphore_p semaphore );
#endif


#endif     // End of #ifndef SHARED_MEMORY_LOCKS_H

/*! @} */

// vim:filetype=h:syntax=c
