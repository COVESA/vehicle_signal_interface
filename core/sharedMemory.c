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

//
//  Set this flag for debugging log messages and timing.
//
#undef DEBUG
// #define DEBUG

#ifdef DEBUG
#   define LOG(...) printf ( __VA_ARGS__ )
#   define HX_DUMP(...) HEX_DUMP_T ( __VA_ARGS__ )
#   define SEM_DUMP(...) dumpSemaphore ( __VA_ARGS__ )
#else
#   define LOG(...)
#   define HX_DUMP(...)
#   define SEM_DUMP(...)
#endif


/*! @{ */


/*!-----------------------------------------------------------------------

    h b _ l o c k

    @brief Acquire the hash bucket lock.

    This function will hang if the lock is currently not available and return
    when the lock has been successfully acquired.

    @param[in] hashBucket - The address of the hash bucket to be locked.

    @return None

------------------------------------------------------------------------*/
static inline void hb_lock ( hashBucket_p hashBucket )
{
    LOG ( "Locking the hash bucket mutex\n" );
    pthread_mutex_lock ( &hashBucket->semaphore.mutex );
}


/*!-----------------------------------------------------------------------

    h b _ u n l o c k

    @brief Release the hash bucket lock.

    This function will unlock the mutex on the specified hash bucket.  If
    another process or thread is waiting on this lock, the scheduler will
    decide which will run so the order of processes/threads may not be the
    same as the order in which they called the lock function.

    @param[in] hashBucket - The address of the hash bucket to be locked.

    @return None

------------------------------------------------------------------------*/
static inline void hb_unlock ( hashBucket_p hashBucket )
{
    LOG ( "Unlocking the hash bucket mutex\n" );
    pthread_mutex_unlock ( &hashBucket->semaphore.mutex );
}


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
sharedMemory_p sm_initialize ( int fd, size_t sharedMemorySegmentSize )
{
	int             status;
	sharedMemory_p sharedMemory;

    LOG ( "Initializing shared memory.  FD[%d], Size[%'lu]\n", fd,
          sharedMemorySegmentSize );
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
	hashBucket_p hashBucket = sm_getBucketAddress ( sharedMemory, 0 );

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

    s m _ i n s e r t

    @brief Insert a new message into the message buffer.

    This function will find the appropriate hash bucket for the given key and
    domain and insert the caller's data into the message list of that hash
    bucket.

    The "key" is an arbitrary integer that uniquely identifies the message
    type within the domain of the message.  Domains are an identifier that
    specifies a class of messages.  For instance, CAN messages can all be
    considered to be messages in the CAN domain.  Within that domain, the
    message ID field uniquely identifies the type of message.

    Note: Unfortunately because of the way the head and tail pointers work in
    this implementation, there are quite a few "special" cases for both the
    insert and delete operations.  In particular, there are special casess
    when the list is empty.

    The "body" pointer supplied by the caller must point to the beginning of
    the data that makes up the message we want to store.  The message size
    specifies the number of bytes that will be read from the user's pointer
    and copied into the shared memory segment.

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

    @return None

------------------------------------------------------------------------*/
void sm_insert ( sharedMemory_p handle, enum domains domain, offset_t key,
                 unsigned long newMessageSize, void* body )
{
    LOG ( "Inserting domain[%d] key[%lu]\n", domain, key );

    //
    //  Define the local message offset and pointer variables.
    //
    offset_t head;
    offset_t tail;

    sharedMessage_p tailPtr;
    sharedMessage_p newMessagePtr;

    offset_t newMessage;
    offset_t newMessageEnd;

    HX_DUMP ( body, newMessageSize, "New Message" );

    //
    //  Compute the message hash value from the input key.
    //
    unsigned long messageHash = sm_getHash ( key );

    //
    //  Get the address of the hash bucket that we will need for the input
    //  key.
    //
    hashBucket_p hashBucket = sm_getBucketAddress ( handle, messageHash );

    //
    //  Acquire the lock on this hash bucket.
    //
    //  Note that this call will hang if someone else is currently using the
    //  hash bucket.  It will return once the lock is acquired and it is safe
    //  to manipulate the hash bucket.
    //
    hb_lock ( hashBucket );

    //
    //  Get the head as the offset to the first message in the message list
    //  and the tail as the offset to the last message in the message list.
    //
    //  NOTE: In the code that follows, the beginning offset is always the
    //  first byte of a message and the end offset is always the last byte of
    //  a message.  The "End" offsets never point to the next available byte.
    //  This was done in case the end of a message buffer happens to fall on a
    //  shared memory segment end address.  In this case, trying to access the
    //  "next" byte could result in a segmention violation and crash the
    //  program.
    //
    head    = hb_getHead ( hashBucket );
    tail    = hb_getTail ( hashBucket );
    tailPtr = hb_getAddress ( hashBucket, tail );

    //
    //  Initialize the flag that is used to determine if the "overflow" check
    //  needs to be done.
    //
    bool checkOverflow = false;

    //
    //  If this is an empty list...
    //
    //  Note that in the case of the first message, this may not be the first
    //  time the list has been empty. It may have had messages in it and all
    //  of those messages were read out making the list empty again but the
    //  head and tail pointers are not at location 0 at that point.
    //
    //  If the message count is zero then both the head and tail offsets must
    //  be the same or we have a problem!  However... The head and tail
    //  offsets both being the same also happens when there is only 1 message
    //  in the buffer, but in this case, the count is not zero.  Therefore, to
    //  test for an empty buffer, use the count value, NOT the head and tail
    //  being the same value.
    //
    //  Legend:
    //
    //    H = Head
    //    T = Tail
    //
    //    Before:                               H/T (Count == 0)
    //            +--------------...-------------+----------...-------------+
    //            |                              |                          |
    //            +--------------...-------------+----------...-------------+
    //
    //            +-----------------+
    //            |-----New Msg-----|
    //            +-----------------+
    //
    //    After: H/T (Count == 1)
    //            +-----------------+---------------...---------------------+
    //            |-----New Msg-----|                                       |
    //            +-----------------+---------------...---------------------+
    //
    //  TODO: This will reset head/tail to the beginning of the list again -
    //  Is that OK??  Doing it this way avoids the edge case where head/tail
    //  are near the end of the buffer and will wrap when a new message is
    //  inserted.  It also has the potential of correcting any minor errors
    //  that may creep into our algorithms.
    //
    if ( hashBucket->currentMessageCount == 0 )
    {
        //
        //  Set the head offset to zero for this first message.
        //
        head = 0;

        //
        //  Set the start offset of new message that we are inserting and the
        //  ending offset of that message.
        //
        newMessage    = 0;
        newMessageEnd = HEADER_SIZE + newMessageSize - 1;
    }
    //
    //  If this is not the empty list case then this case should be the plain
    //  old "normal" case in which we will just insert the new message after
    //  the "tail" message that is currently in the buffer.
    //
    //  This is also the case when the tail pointer has wrapped around and is
    //  now less than the head pointer.
    //
    //    Before:           H          T
    //            +---------+----------+-------+----------------------------+
    //            |         |---msgs---|--msg--|                            |
    //            +---------+----------+-------+----------------------------+
    //
    //                                         +-----------------+
    //                                         |-----New Msg-----|
    //                                         +-----------------+
    //
    //    After:            H                  T
    //            +---------+----------+-------+-----------------+----------+
    //            |         |---msgs---|--msg--|-----New Msg-----|          |
    //            +---------+----------+-------+-----------------+----------+
    //
    else
    {
        //
        //  Set the beginning and ending offsets of the new message that we
        //  will by trying to insert into the list.
        //
        newMessage = tail + HEADER_SIZE + tailPtr->messageSize;
        newMessageEnd = newMessage + HEADER_SIZE + newMessageSize - 1;

        //
        //  Set the flag to enable the overflow checking later on in this
        //  function that detects when we will overflow the buffer capacity
        //  when we try to insert this new message.
        //
        checkOverflow = true;
    }
    //
    //  If the new message will extend past the end of the message list...
    //  Then there is not enough room at the end of the list to insert the new
    //  message so we need to insert the new message at the beginning of the
    //  list.
    //
    //  Note that when we wrap, there will likely be some unused space between
    //  the last message in the buffer and the end of the buffer.  This space
    //  is accounted for by updating the "nextMessageOffset" of the last
    //  buffer to point to the beginning of the buffer again.  This unused
    //  space will be reused as needed by subsequent messages and will not be
    //  lost.
    //
    //    Before:                     H          T
    //            +------...----------+----------+-------+---+
    //            |                   |---msgs---|--msg--|   |
    //            +------...----------+----------+-------+---+
    //
    //                                                   +-----------+
    //                                                   |--New Msg--|
    //                                                   +-----------+
    //
    //    After:  T                   H
    //            +-----------+--...--+------------------+---+
    //            |--New Msg--|       |-------msgs-------|   |
    //            +-----------+--...--+------------------+---+
    //
    if ( newMessageEnd >= hashBucket->totalMessageSize )
    {
        //
        //  Set the new message to be inserted at the beginning of the message
        //  list.
        //
        newMessage    = 0;
        newMessageEnd = HEADER_SIZE + newMessageSize - 1;

        //
        //  Increment the "generation" counter to allow apps to tell if there
        //  have been messages lost since they last checked the messages.
        //
        ++hashBucket->generationNumber;

        //
        //  Set the flag to enable the overflow checking later on in this
        //  function that detects when we will overflow the buffer capacity
        //  when we try to insert this new message.
        //
        checkOverflow = true;
    }
    //
    //  If the new message begins before the head location and will extend
    //  past the head location... The list is full and we need to move the
    //  head location forward to make room for the new message.  Note that
    //  this will discard some number of messages at the original head
    //  location.
    //
    //  This case also happens often when a message overflows the end of the
    //  message buffer, so in that case, we need to apply this logic after the
    //  wrap-around has taken place.
    //
    //  TODO: There is a small possibility that while adjusting the head
    //  pointer, we could run off the end of the message list.  There is also
    //  an even smaller possibility that the new message will not only overlap
    //  the head pointer but also the end of the message list itself.  Check
    //  these cases!
    //
    //    Before:           T            H
    //            +---------+--------+---+----------------------------------+
    //            |---msgs--|--msg---|   |--------------msgs----------------|
    //            +---------+--------+---+----------------------------------+
    //
    //                               +-----------------+
    //                               |-----New Msg-----|
    //                               +-----------------+
    //
    //    After:                     T                       H
    //            +---------+--------+-----------------+-----+--------------+
    //            |-------msgs-------|-----New Msg-----|     |-----msgs-----|
    //            +---------+--------+-----------------+-----+--------------+
    //
    if ( ( ( newMessage < head ) && ( newMessageEnd >= head ) ) ||
         ( checkOverflow &&
           ( newMessage <= head ) && ( newMessageEnd >= head ) ) )
    {
        //
        //  Move the head location forward in the list until there is enough
        //  room to insert the new message...
        //
        while ( head <= newMessageEnd && head != END_OF_BUCKET_DATA )
        {
            //
            //  Move the head offset to the beginning of the next message in
            //  the list.
            //
            head = ( hb_getAddress ( hashBucket, head ) )->nextMessageOffset;

            //
            //  Decrement the number of messages in the message list because
            //  we have just thrown one of them away.
            //
            --hashBucket->currentMessageCount;

            //
            //  If the head offset is now at the beginning of the buffer, we
            //  are done and can stop throwing messages away.
            //
            if ( head == 0 )
            {
                break;
            }
        }
        //
        //  If the end of the message list was reached then stop and reset the
        //  head location to the beginning of the message buffer.
        //
        if ( head == END_OF_BUCKET_DATA )
        {
            head = 0;
        }
    }
    //
    //  Now get a pointer to the last message in the message list.
    //
    tailPtr->nextMessageOffset = newMessage;

    //
    //  Reset the tail offset to the beginning of the new message we are in
    //  the process of inserting.
    //
    tail = newMessage;

    //
    //  Get the address of where the new message is located in the buffer.
    //
    newMessagePtr = hb_getAddress ( hashBucket, newMessage );

    //
    //  Initialize all of the fields in the message header of the new message
    //  that we are inserting.
    //
    newMessagePtr->key               = key;
    newMessagePtr->domain            = domain;
    newMessagePtr->messageSize       = newMessageSize;
    newMessagePtr->nextMessageOffset = END_OF_BUCKET_DATA;

    //
    //  Copy the message body into the message list.
    //
    //  TODO: Validate that the message size is "reasonable"!
    //
    memcpy ( (void*)(&newMessagePtr->data), body, newMessageSize );

    //
    //  Update the head and tail offsets for this hash bucket.
    //
    hashBucket->head = head;
    hashBucket->tail = tail;

    //
    //  Increment the message count and message sequence numbers.
    //
    ++hashBucket->currentMessageCount;
    ++hashBucket->messageSequenceNumber;

    //
    //  Increment the message count in the semaphore.
    //
    ++hashBucket->semaphore.messageCount;

    //
    //  Increment the semaphore count to reflect the message we just inserted
    //  into the message list.
    //
    LOG ( "%'lu  Before Insert/semaphore post:\n", getIntervalTime() );
    SEM_DUMP ( "     ", &hashBucket->semaphore );

    semaphorePost ( &hashBucket->semaphore );

    LOG ( "%'lu  After Insert/semaphore post:\n", getIntervalTime() );
    SEM_DUMP ( "     ", &hashBucket->semaphore );

    //
    //  Give up the hash bucket memory lock.
    //
    hb_unlock ( hashBucket );

    //
    //  Return to the caller.
    //
    return;
}


/*!-----------------------------------------------------------------------

	s m _ r e m o v e M e s s a g e

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
int sm_removeMessage ( hashBucket_p hashBucket, offset_t currentMessage,
                       offset_t previousMessage )
{
	sharedMessage_p currentPtr;
	sharedMessage_p previousPtr;

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


/*!-----------------------------------------------------------------------

    s m _ f e t c h

    @brief Retrieve a message from the message buffer.

    This function will attempt to retrieve a message with the specified
    key and domain from the shared memory message buffer.  The message that
    was found (and returned to the caller) will be removed from the message
    list.  If there are no messages that match the user's request, this
    function will hang (on a semaphore) until an appropriate message is
    available before returning to the caller.

    When calling this function, the messageSize should be the size of the data
    buffer supplied as the "body" argument.  When we copy the data from the
    shared memory segment to the "body" buffer, the smaller of either the body
    buffer size or the number of bytes of data in the message found will be
    copied into the body buffer.

    @param[in]  handle - The base address of the shared memory segment.
    @param[in]  domain - The domain value of the message to be removed.
    @param[in]  key - The key value of the message to be removed.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the message data.
    @param[in]  dontWait - If true, don't wait for data if domain/key is not found.

    @return 0 if successful.
            -ENODATA - If waitForData == true and domain/key is not found.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_fetch ( sharedMemory_p handle, enum domains domain, offset_t key,
               unsigned long* bodySize, void* body, bool dontWait )
{
    //
    //  Define the local message offset and pointer variables.
    //
    offset_t         currentMessage;
    offset_t         previousMessage;

    sharedMessage_p messagePtr;

    int              transferSize;
    int              status = 0;

    LOG ( "Fetching signal domain[%d], key[%lu], dontWait[%d]\n", domain, key,
          dontWait );
    //
    //  Compute the message hash value from the input key.
    //
    unsigned long messageHash = sm_getHash ( key );

    //
    //  Get the address of the hash bucket that we will need for the input
    //  key.
    //
    hashBucket_p hashBucket = sm_getBucketAddress ( handle, messageHash );


    //
    //  If this hash bucket is empty and we don't want to wait for the data to
    //  arrive then just return the "no data" error code to the caller and
    //  quit.
    //
    if ( hashBucket->semaphore.messageCount == 0 && dontWait )
    {
        return -ENODATA;
    }
    //
    //  Acquire the lock on the shared memory segment.
    //
    //  Note that this call will hang if someone else is currently using the
    //  shared memory segment.  It will return once the lock is acquired and it
    //  is safe to manipulate the shared memory data.
    //
    hb_lock ( hashBucket );

    //
    //  Go wait if there are no messages in the message list to be read.
    //
    //  Note that we will increment the "waiter" count during the time that we
    //  are potentially waiting on the semaphore and decrement it again when
    //  we come back from the wait.  This is used to tell the "fetch" logic
    //  when it is OK to delete the message that is being waited on.  If more
    //  than one process is waiting on this message then we want to give the
    //  message to all of the processes that are waiting before we delete the
    //  message from the message list.
    //
    ++hashBucket->semaphore.waiterCount;

    LOG ( "%'lu  Before Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( "  ", &hashBucket->semaphore );

    semaphoreWait ( &hashBucket->semaphore );

    --hashBucket->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( "  ", &hashBucket->semaphore );

    //
    //  Get the current and previous message offsets.  Note that for the first
    //  time through the loop, both of these values will be the same.
    //
    currentMessage = previousMessage = hb_getHead ( hashBucket );

    //
    //  Repeat until we reach the end of the message list in this bucket.
    //
    while ( currentMessage != END_OF_BUCKET_DATA )
    {
        //
        //  Get the actual memory pointer to the current message.
        //
        messagePtr =  hb_getAddress ( hashBucket, currentMessage );

        //
        //  If this message is the one we want (i.e. the key and domain are
        //  the same as the ones requested by the caller)...
        //
        if ( key == messagePtr->key && domain == messagePtr->domain )
        {
            //
            //  Copy the message body into the caller supplied buffer.
            //
            //  Note that we will copy the SMALLER of either the message size
            //  supplied by the caller or the actual data size in the message
            //  that was found.
            //
            transferSize = *bodySize <= messagePtr->messageSize
                           ? *bodySize : messagePtr->messageSize;

            memcpy ( body, messagePtr->data, transferSize );

            //
            //  Return the number of bytes that were copied to the caller.
            //
            *bodySize = transferSize;

            //
            //  If no one else is now waiting for this message, go remove this
            //  message from the message list.
            //
            if ( hashBucket->semaphore.waiterCount <= 0 )
            {
                status = sm_removeMessage ( hashBucket, currentMessage,
                                            previousMessage );
            }
            //
            //  Stop searching for the message.
            //
            break;
        }
        //
        //  Increment our message offsets to the next message in the list.
        //
        previousMessage = currentMessage;
        currentMessage = messagePtr->nextMessageOffset;
    }
    LOG ( "%'lu  At the end of Fetch - waiterCount: %d\n", getIntervalTime(),
             hashBucket->semaphore.waiterCount );

    SEM_DUMP ( "     ", &hashBucket->semaphore );

    //
    //  If we did not find the message we were looking for, return an error
    //  code to the caller.
    //
    if ( currentMessage == END_OF_BUCKET_DATA )
    {
        status = -ENODATA;
    }
    //
    //  Give up the shared memory block lock.
    //
    hb_unlock ( hashBucket );

    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ f e t c h _ n e w e s t

    @brief Retrieve the newest message from the message buffer.

    This function will attempt to retrieve the newest message with the specified
    key and domain from the shared memory message buffer.  The message that
    is found (and returned to the caller) will NOT be removed from the message
    list.  If there are no messages that match the user's request, this
    function will hang (on a semaphore) until an appropriate message is
    available before returning to the caller depending on the setting of the
    "dontWait" input parameter.

    When calling this function, the messageSize should be the size of the data
    buffer supplied as the "body" argument.  When we copy the data from the
    shared memory segment to the "body" buffer, the smaller of either the body
    buffer size or the number of bytes of data in the message found will be
    copied into the body buffer.  The actual number of data bytes copied will
    be return to the caller in the bodySize variable.

    @param[in]  handle - The base address of the shared memory segment.
    @param[in]  domain - The domain value of the message to be removed.
    @param[in]  key - The key value of the message to be removed.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the message data.
    @param[in]  dontWait - If true, don't wait for data if domain/key is not found.

    @return 0 if successful.
            -ENODATA - If waitForData == true and domain/key is not found.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_fetch_newest ( sharedMemory_p handle, enum domains domain, offset_t key,
                      unsigned long* bodySize, void* body, bool dontWait )
{
    //
    //  Define the local message offset and pointer variables.
    //
    offset_t        currentMessage = 0;

    sharedMessage_p messagePtr = 0;
    sharedMessage_p lastMessagePtr = 0;

    int             transferSize = 0;
    int             status = -ENODATA;

    LOG ( "Fetching newest signal domain[%d], key[%lu], dontWait[%d]\n", domain,
          key, dontWait );
    //
    //  Compute the message hash value from the input key.
    //
    unsigned long messageHash = sm_getHash ( key );

    //
    //  Get the address of the hash bucket that we will need for the input
    //  key.
    //
    hashBucket_p hashBucket = sm_getBucketAddress ( handle, messageHash );


    //
    //  If this hash bucket is empty and we don't want to wait for the data to
    //  arrive then just return the "no data" error code to the caller and
    //  quit.
    //
    if ( hashBucket->semaphore.messageCount == 0 && dontWait )
    {
        LOG ( "Returning error code ENODATA\n" );
        return -ENODATA;
    }
    //
    //  Acquire the lock on the shared memory segment.
    //
    //  Note that this call will hang if someone else is currently using the
    //  shared memory segment.  It will return once the lock is acquired and it
    //  is safe to manipulate the shared memory data.
    //
    hb_lock ( hashBucket );

    //
    //  Go wait if there are no messages in the message list to be read.
    //
    //  Note that we will increment the "waiter" count during the time that we
    //  are potentially waiting on the semaphore and decrement it again when
    //  we come back from the wait.  This is used to tell the "fetch" logic
    //  when it is OK to delete the message that is being waited on.  If more
    //  than one process is waiting on this message then we want to give the
    //  message to all of the processes that are waiting before we delete the
    //  message from the message list.
    //
    ++hashBucket->semaphore.waiterCount;

    LOG ( "%'lu  Before Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( "  ", &hashBucket->semaphore );

    semaphoreWait ( &hashBucket->semaphore );

    --hashBucket->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( "  ", &hashBucket->semaphore );

    //
    //  If we get here, there is at least one message in this hash bucket.  At
    //  that point, we can take a look at the last message in the bucket.  If
    //  the last message matches the domain/key we are looking for then we are
    //  done and can just return that.  If not, we will need to search the
    //  entire hash bucket for the last match.  The list is one-way so we
    //  can't traverse it from the end backwards.
    //
    currentMessage = hb_getTail ( hashBucket );

    //
	//  Get the actual memory pointer to the last message in the list.
	//
	messagePtr =  hb_getAddress ( hashBucket, currentMessage );

    //
    //  If this message matches the key and domain we need then we are almost
    //  done...
    //
	if ( key == messagePtr->key && domain == messagePtr->domain )
	{
        //
        //  Copy the message body into the caller supplied buffer.
        //
        //  Note that we will copy the SMALLER of either the message size
        //  supplied by the caller or the actual data size in the message
        //  that was found.
        //
        transferSize = *bodySize <= messagePtr->messageSize
                       ? *bodySize : messagePtr->messageSize;

        memcpy ( body, messagePtr->data, transferSize );

        //
        //  Return the number of bytes that were copied to the caller.
        //
        *bodySize = transferSize;

        //
        //  Return to the caller with a good completion code.
        //
        LOG ( "Returning error code 0\n" );
        return 0;
    }
    //
    //  If we get here it's because the last message in the current hash
    //  bucket does not match the key and domain we are looking for.  In this
    //  case, we will need to search the entire list from the beginning,
    //  looking for the last match.
    //
    //  Get the current and previous message offsets.  Note that for the first
    //  time through the loop, both of these values will be the same.
    //
    currentMessage = hb_getHead ( hashBucket );

    //
    //  Repeat until we reach the end of the message list in this bucket.
    //
    while ( currentMessage != END_OF_BUCKET_DATA )
    {
        //
        //  Get the actual memory pointer to the current message.
        //
        messagePtr =  hb_getAddress ( hashBucket, currentMessage );

        //
        //  If this message is the one we want (i.e. the key and domain are
        //  the same as the ones requested by the caller)...
        //
        if ( key == messagePtr->key && domain == messagePtr->domain )
        {
            //
            //  Save the pointer to this message in case it is the last one we
            //  find.
            //
            lastMessagePtr = messagePtr;
        }
        //
        //  Increment our message offsets to the next message in the list.
        //
        currentMessage = messagePtr->nextMessageOffset;
    }
    LOG ( "%'lu  At the end of Fetch - waiterCount: %d\n", getIntervalTime(),
          hashBucket->semaphore.waiterCount );

    SEM_DUMP ( "     ", &hashBucket->semaphore );

    //
    //  If we did not find the message we were looking for, return an error
    //  code to the caller indicating that we did not find the message he
    //  wanted.
    //
    if ( lastMessagePtr != 0 )
    {
        //
        //  Copy the message body into the caller supplied buffer.
        //
        //  Note that we will copy the SMALLER of either the message size
        //  supplied by the caller or the actual data size in the message
        //  that was found.
        //
        transferSize = *bodySize <= lastMessagePtr->messageSize
                       ? *bodySize : lastMessagePtr->messageSize;

        memcpy ( body, lastMessagePtr->data, transferSize );

        //
        //  Return the number of bytes that were copied to the caller.
        //
        *bodySize = transferSize;

        //
        //  Return a good completion code to the caller.
        //
        status = 0;
    }
    //
    //  Give up the shared memory block lock.
    //
    hb_unlock ( hashBucket );

    //
    //  Return the completion code to the caller.
    //
    LOG ( "Returning error code %d\n", status );
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ f l u s h _ s i g n a l

    @brief Flush all specified signals from the data store.

    This function will find all signals in the data store with the specified
    domain and key value and remove them from the data store.

    @param[in]  handle - The base address of the shared memory segment.
    @param[in]  domain - The domain value of the message to be removed.
    @param[in]  key - The key value of the message to be removed.

    @return 0 if successful.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_flush_signal ( sharedMemory_p handle, enum domains domain, offset_t key )
{
    //
    //  Define the local message offset and pointer variables.
    //
    offset_t         currentMessage;
    offset_t         previousMessage;
    offset_t         nextMessage;

    sharedMessage_p messagePtr;

    int              status = 0;

    LOG ( "Flushing signal domain[%d], key[%lu]\n", domain, key );

    //
    //  Compute the message hash value from the input key.
    //
    unsigned long messageHash = sm_getHash ( key );

    //
    //  Get the address of the hash bucket that we will need for the input
    //  key.
    //
    hashBucket_p hashBucket = sm_getBucketAddress ( handle, messageHash );

    //
    //  If this hash bucket is empty then we don't need to do anything.  In
    //  this case, just return a good completion code.
    //
    if ( hashBucket->semaphore.messageCount == 0 )
    {
        return 0;
    }
    //
    //  Acquire the lock on the shared memory segment.
    //
    //  Note that this call will hang if someone else is currently using the
    //  shared memory segment.  It will return once the lock is acquired and it
    //  is safe to manipulate the shared memory data.
    //
    hb_lock ( hashBucket );

    //
    //  Get the current and previous message offsets.  Note that for the first
    //  time through the loop, both of these values will be the same.
    //
    currentMessage = previousMessage = hb_getHead ( hashBucket );
    nextMessage = END_OF_BUCKET_DATA;

    //
    //  Repeat until we reach the end of the message list in this bucket.
    //
    while ( currentMessage != END_OF_BUCKET_DATA )
    {
        //
        //  Get the actual memory pointer to the current message.
        //
        messagePtr =  hb_getAddress ( hashBucket, currentMessage );

        //
        //  Save the offset of the next message before we possibly destroy the
        //  current message.
        //
        nextMessage = messagePtr->nextMessageOffset;

        //
        //  If this message is the one we want (i.e. the key and domain are
        //  the same as the ones requested by the caller)...
        //
        if ( key == messagePtr->key && domain == messagePtr->domain )
        {
            status |= sm_removeMessage ( hashBucket, currentMessage,
                                         previousMessage );
        }
        //
        //  Increment our message offsets to the next message in the list.
        //
        previousMessage = currentMessage;
        currentMessage = nextMessage;
    }
    //
    //  If anyone is waiting for this message, go release them to search the
    //  message list again.
    //
    if ( hashBucket->semaphore.waiterCount > 0 )
    {
        semaphorePost ( &hashBucket->semaphore );
    }
    //
    //  Give up the shared memory block lock.
    //
    hb_unlock ( hashBucket );

    //
    //  Return the completion code to the caller.
    //
    return status;
}



/*! @} */

// vim:filetype=c:syntax=c
