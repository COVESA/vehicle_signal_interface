/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file      semaphore.c

    This file contains the definitions of all of the semaphore objects and
    member functions.

-----------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include "vsi_core_api.h"
#include "sharedMemoryLocks.h"
#include "utils.h"


/*! @{ */


/*!-----------------------------------------------------------------------

    s e m a p h o r e P o s t

    @brief Perform a "post" operation on a semaphore.

    This function will perform a "post" operation on a semaphore object
    (similar to a "signal" on a mutex).  This operation will increment the
    semaphore's count value and if there are any processes waiting on this
    semahore, one (or possibly more) of them will be released and allowed to
    run.  Under normal conditions, only one waiting process will be released
    when this signal occurs but there is a very small window in which more
    than one could be released.  This is not a problem - See the man page for
    pthread_cond_signal.

    That this code assumes that the semaphore object to be operated on already
    exists and has been mapped into this process's shared memory segment.  The
    semaphore must already have been initialized as well.

    @param[in] semaphore - The address of the semaphore object to operate on.

------------------------------------------------------------------------*/
void semaphorePost ( semaphore_p semaphore )
{
    int status;

    //
    //  If the semaphore count is at 0 then someone may be currently waiting
    //  on this semaphore so just signal the condition variable to indicate
    //  that the resource is available and release the process(s) that are
    //  waiting.
    //
    pthread_mutex_lock ( &semaphore->mutex );

    LOG ( "Before semaphore broadcast (post) of sem: %p\n", semaphore );
    SEM_DUMP ( semaphore );

    status = pthread_cond_broadcast ( &semaphore->conditionVariable );
    if ( status != 0 )
    {
        printf ( "Unable to broadcast to condition variable - "
                 "errno: %u[%s].\n", status, strerror(status) );
    }
    pthread_mutex_unlock ( &semaphore->mutex );

    LOG ( "After semaphore broadcast of %p:\n", semaphore );
    SEM_DUMP ( semaphore );
}


//
//  Define the cleanup handler for the semaphore wait below.  This handler
//  will be executed if this thread is cancelled while it is waiting.  What
//  this function needs to do is release the mutex that this thread is holding
//  when it enters the wait state.  Without this cleanup handler, the mutex
//  will remain locked when this thread is cancelled and almost certainly
//  cause a hang condition in the code the next time someone tries to lock
//  this same mutex.
//
static void semaphoreCleanupHandler ( void* arg )
{
    pthread_mutex_unlock ( arg );
}


void semaphoreWait ( semaphore_p semaphore )
{
    int status;

    //
    //  If the semaphore count is at 0 then the resource is not available so
    //  we need to wait for it to become available.  This is done in a "while"
    //  loop because we could be released spuriously due to a very small
    //  window in the semaphore code that results in more than one process
    //  being released at times.  By checking the semaphore again after being
    //  released, we guarantee that only one process will actually acquire the
    //  semaphore.
    //
    pthread_mutex_lock ( &semaphore->mutex );

    LOG ( "Before semaphore wait on sem: %p\n", semaphore );
    SEM_DUMP ( semaphore );

    while ( semaphore->messageCount == 0 )
    {
        LOG ( "In semaphore while[%p] - waiterCount: %d, messageCount: %d\n",
              semaphore, semaphore->waiterCount, semaphore->messageCount );
        //
        //  Install the cancellation cleanup handler function.
        //
        pthread_cleanup_push ( semaphoreCleanupHandler, &semaphore->mutex );

        status = pthread_cond_wait ( &semaphore->conditionVariable,
                                     &semaphore->mutex );
        if ( status != 0 )
        {
            printf ( "Unable to wait on condition variable - "
                     "errno: %u[%s].\n", status, strerror(status) );
        }
        //
        //  Release the cancellation cleanup handler function.
        //
        pthread_cleanup_pop ( 0 );
    }
    pthread_mutex_unlock ( &semaphore->mutex );

    LOG ( "After semaphore wait on %p:\n", semaphore );
    SEM_DUMP ( semaphore );
}

/*! @} */

// vim:filetype=c:syntax=c
