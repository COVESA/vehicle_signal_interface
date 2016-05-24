/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

	@file sharedMemory.c

	This file contains the functions that manipulate the shared memory segment
	data structures.  Most of the major functions defined in the VSI are
	defined here.

-----------------------------------------------------------------------------*/

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "sharedMemory.h"
#include "utils.h"

// #define LOG(...) printf ( __VA_ARGS__ )
#define LOG(...)

/*! @{ */


/*!-----------------------------------------------------------------------

    s m _ i n i t i a l i z e

	@brief Initialize the data structures in a new shared memory segment.

	The specified file (referenced by the supplied file descriptor) will be
	mapped into virtual memory and the size of the file adjusted to match the
	current size of the shared memory segment.

	This function will set up all of the data structures in the shared memory
	segment for use.  This function should be called after a brand new shared
	memory segment has been created and opened on disk.  This function should
	not be called on an existing shared memory segment that contains data as
	all of that data will be deleted by this function.

	@param[in] fd - The file descriptor of the file associated with this
			   shared memory segment.

	@param[in] sharedMemorySegmentSize - The size of the shared memory
			   segment in bytes.

	@return The new memory address of where the shared memory segment begins.

	        On error, a null pointer is returned and errno will be set to
			indicate the error that was encountered.

------------------------------------------------------------------------*/
sharedMemory_t* sm_initialize ( int fd, size_t sharedMemorySegmentSize )
{
	int             status;
	sharedMemory_t* sharedMemory;

	//
	//	Map the shared memory file into virtual memory.
	//
	sharedMemory = mmap ( NULL, TOTAL_SHARED_MEMORY_SIZE, PROT_READ|PROT_WRITE,
					      MAP_SHARED, fd, 0 );
	if ( sharedMemory == MAP_FAILED )
	{
		printf ( "Unable to map the shared memory segment. errno: %u[%m].\n",
				 errno );
		return 0;
	}
    //
	//	Make the pseudo-file for the shared memory segment the size of the
	//	shared memory segment.  Note that this will destroy any existing data
	//	if the segment already exists.
    //
    status = ftruncate ( fd, TOTAL_SHARED_MEMORY_SIZE );
    if (status != 0)
    {
        printf ( "Unable to resize the shared memory segment to [%lu] bytes - "
                 "errno: %u[%m].\n", TOTAL_SHARED_MEMORY_SIZE, errno );
        return 0;
    }
	//
	//	Set all of the data in this shared memory segment to zero.
	//
	( void ) memset ( sharedMemory, 0, TOTAL_SHARED_MEMORY_SIZE );

	//
	//	Initialize all of the hash buckets in the shared memory segment.
	//
	//	Create the mutex attribute initializer that we can use to initialize
	//	all of the mutexes in the hash buckets.
    //
	pthread_mutexattr_t mutexAttributes;

    status =  pthread_mutexattr_init ( &mutexAttributes );
    if ( status != 0 )
    {
        printf ( "Unable to initialize mutex attributes - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    // Set this mutex to be a process shared mutex.
    //
    status = pthread_mutexattr_setpshared ( &mutexAttributes,
											PTHREAD_PROCESS_SHARED );
    if ( status != 0 )
    {
        printf ( "Unable to set shared mutex attribute - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    // Set this mutex to be a recursive mutex.
    //
    status = pthread_mutexattr_settype ( &mutexAttributes,
										 PTHREAD_MUTEX_RECURSIVE );
    if ( status != 0 )
    {
        printf ( "Unable to set recursive mutex attribute - errno: %u[%m].\n",
                 status );
        return 0;
    }
	//
	//	Create the condition variable initializer that we can use to
	//	initialize all of the mutexes in the hash buckets.
	//
	pthread_condattr_t conditionVariableAttributes;

	status = pthread_condattr_init ( &conditionVariableAttributes );

    if ( status != 0 )
    {
        printf ( "Unable to initialize condition variable attributes - "
			     "errno: %u[%m].\n", status );
        return 0;
    }
	//
	//	Set this condition variable to be shared between processes.
	//
	status = pthread_condattr_setpshared ( &conditionVariableAttributes,
											PTHREAD_PROCESS_SHARED );
    if ( status != 0 )
    {
        printf ( "Unable to set shared condition variable attributes - "
				 "errno: %u[%m].\n", status );
        return 0;
    }
	//
	//	For each hash bucket in our hash table...
	//
	hashBucket_t* hashBucket = sm_getBucketAddress ( sharedMemory, 0 );

	for ( int i = 0; i < HASH_BUCKET_COUNT; ++i )
	{
		//
		//	Initialize all of the hash table variables.
		//
		hashBucket->head                  = 0;
		hashBucket->tail                  = 0;
		hashBucket->currentMessageCount   = 0;
		hashBucket->generationNumber      = 0;
		hashBucket->messageSequenceNumber = 0;
		hashBucket->totalMessageSize      = HASH_BUCKET_DATA_SIZE;

		//
		//	Initialize the hash bucket data lock mutex.
		//
		status = pthread_mutex_init ( &hashBucket->lock, &mutexAttributes );
		if ( status != 0 )
		{
			printf ( "Unable to initialize hash bucket data lock mutex - "
			 		 "errno: %u[%m].\n", status );
			return 0;
		}
		//
		//	Initialize the hash bucket semaphore mutex.
		//
		status = pthread_mutex_init ( &hashBucket->semaphore.mutex,
									  &mutexAttributes );
		if ( status != 0 )
		{
			printf ( "Unable to initialize hash bucket mutex - errno: %u[%m].\n",
					 status );
			return 0;
		}
		status = pthread_cond_init ( &hashBucket->semaphore.conditionVariable,
									 &conditionVariableAttributes );
		if ( status != 0 )
		{
			printf ( "Unable to initialize hash bucket condition variable - "
					 "errno: %u[%m].\n", status );
			return 0;
		}
		//
		//	Initialize the semaphore counters for this hash bucket.
		//
		hashBucket->semaphore.messageCount = 0;
		hashBucket->semaphore.waiterCount  = 0;

		//
		//	Increment to the next hash bucket.
		//
		++hashBucket;
	}
	//
	//	Initialize the global time value.  TODO: Debug only!
	//
	// sharedMemory->globalTime = 0;

	//
	//	Return the address of the shared memory segment to the caller.
	//
	return sharedMemory;
}


/*!-----------------------------------------------------------------------

	r e m o v e M e s s a g e

	@brief Remove the "current" message from the message list.

	This function will remove the message at the specified offset from the
	message list for the specified hash bucket.

	WARNING: This function assumes that the hash bucket mutex has been
	acquired before this function is called.

	@param[in] hashBucket - The address of the hash bucket to operate on.
	@param[in] currentMessage - The offset of the message to remove.
	@param[in] previousMessage - The offset of the previous message.

	@return 0 - Message successfully removed.

------------------------------------------------------------------------*/
int removeMessage ( hashBucket_t* hashBucket, offset_t currentMessage,
                    offset_t previousMessage )
{
	sharedMessage_t* currentPtr;
	sharedMessage_t* previousPtr;

	LOG ( "%'lu  Removing message...\n", getIntervalTime() );

	//
	//	If this is the first message in the list...
	//
	currentPtr = hb_getAddress ( hashBucket, currentMessage );
	if ( currentMessage == hashBucket->head )
	{
		//
		//	Just make the head offset be the offset of the next message after
		//	the one we are removing.
		//
		hashBucket->head = currentPtr->nextMessageOffset;
	}
	//
	//	Otherwise, if this is not the first message in the list...
	//
	else
	{
		//
		//	Make the "next" offset in the previous message be the "next"
		//	offset in the message we are going to remove.
		//
		previousPtr = hb_getAddress ( hashBucket, previousMessage );
		previousPtr->nextMessageOffset = currentPtr->nextMessageOffset;
	}
	//
	//	Decrement the count of the number of messages in this list.
	//
	--hashBucket->currentMessageCount;

	//
	//	Decrement the count of the number of messages in the list within the
	//	semaphore.
	//
	--hashBucket->semaphore.messageCount;

	//
	//	Return a good completion code to the caller.
	//
	return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
