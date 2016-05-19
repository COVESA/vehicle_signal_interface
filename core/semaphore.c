/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

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

#include "semaphore.h"
#include "utils.h"

// #define LOG(...) printf ( __VAR_ARGS__ )
#define LOG(...)

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
void semaphorePost ( semaphore_t* semaphore )
{
	int status;

	//
	//	Obtain the lock on the semaphore's mutex.
	//
	// pthread_mutex_lock ( &semaphore->mutex );

	//
	//	If the semaphore count is at 0 then someone may be currently waiting
	//	on this semaphore so just signal the condition variable to indicate
	//	that the resource is available and release the process(s) that are
	//	waiting.
	//
	LOG ( "%'lu  Before semaphore broadcast:\n", getIntervalTime() );
	dumpSemaphore ( "     ", semaphore );

	status = pthread_cond_broadcast ( &semaphore->conditionVariable );
	if ( status != 0 )
	{
		printf ( "Unable to broadcast to condition variable - "
				 "errno: %u[%s].\n", status, strerror(status) );
	}

	LOG ( "%'lu  After semaphore broadcast:\n", getIntervalTime() );
	dumpSemaphore ( "     ", semaphore );

	//
	//	Give up the lock on the semaphore's mutex to give others a chance to
	//	play with this semaphore.
	//
	// pthread_mutex_unlock ( &semaphore->mutex );
}


void semaphoreWait ( semaphore_t* semaphore )
{
	int status;

	//
	//	Obtain the lock on the semaphore's mutex.
	//
	// pthread_mutex_lock ( &semaphore->mutex );

	//
	//	If the semaphore count is at 0 then the resource is not available so
	//	we need to wait for it to become available.  This is done in a "while"
	//	loop because we could be released spuriously due to a very small
	//	windown in the semaphore code that results in more than one process
	//	being released at times.  By checking the semaphore again after being
	//	released, we guarantee that only one process will actually acquire the
	//	semaphore.
	//
	LOG ( "%'lu  Before semaphore wait:\n", getIntervalTime() );
	dumpSemaphore ( "  ", semaphore );

	while ( semaphore->messageCount == 0 )
	{
		LOG ( "%'lu    In semaphore while[%p] - waiterCount: %d, messageCount: %d\n",
				 getIntervalTime(), semaphore, semaphore->waiterCount,
				 semaphore->messageCount );
		status = pthread_cond_wait ( &semaphore->conditionVariable,
									 &semaphore->mutex );
		if ( status != 0 )
		{
			printf ( "Unable to wait on condition variable - "
					 "errno: %u[%s].\n", status, strerror(status) );
		}
	}

	LOG ( "%'lu  After semaphore wait:\n", getIntervalTime() );
	dumpSemaphore ( "  ", semaphore );

	//
	//	Give up the lock on the semaphore's mutex to give others a chance to
	//	play with this semaphore.
	//
	// pthread_mutex_unlock ( &semaphore->mutex );
}

/*! @} */

// vim:filetype=c:syntax=c
