/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file vsi_core_api.c

    This file contains the publically available functions that allow users to
    interract with the VSI core data store.

-----------------------------------------------------------------------------*/

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "vsi_core_api.h"

// #define LOG(...) printf ( __VA_ARGS__ )
#define LOG(...)

/*! @{ */


/*!-----------------------------------------------------------------------

    h b _ l o c k

    @brief Acquire the hash bucket lock.

    This function will hang if the lock is currently not available and return
    when the lock has been successfully acquired.

    @param[in] hashBucket - The address of the hash bucket to be locked.

    @return None

------------------------------------------------------------------------*/
static inline void hb_lock ( hashBucket_t* hashBucket )
{
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
static inline void hb_unlock ( hashBucket_t* hashBucket )
{
    pthread_mutex_unlock ( &hashBucket->semaphore.mutex );
}


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ o p e n

    @brief Open the shared memory segment.

    This function will attempt to open the configured shared memory segment.
    If the shared memory segment pseudo-file does not exist or is empty, a new
    shared memory segment will be initialized with all of the appropriate
    parameters and values set.

    Note that the base address of the shared memory segment will vary and
    cannot be guaranteed to be the same from one invocation to another.  All
    references to data in the shared memory segment MUST be made with offsets
    relative to the beginning of the shared memory segment and when a real
    pointer is needed (for accessing a particular data buffer for instance),
    the address of the desired data must be dynamically calculated by adding
    the base address of the shared memory segment to any offset that is
    desired.

    @return A handle to the shared memory segement that was opened.

            On error, a null pointer is returned and errno will be set to
            indicate the error that was encountered.

------------------------------------------------------------------------*/
vsi_core_handle vsi_core_open ( void )
{
    //
    //  Open the shared memory segment file and verify that it opened
    //  properly.
    //
    int fd = 0;
    fd = open ( SHARED_MEMORY_SEGMENT_NAME, O_RDWR|O_CREAT, 0666);
    if (fd < 0)
    {
        printf ( "Unable to open the VSI core data store[%s] errno: %u[%m].\n",
                 SHARED_MEMORY_SEGMENT_NAME, errno );
        return 0;
    }
    //
    //  Go get the size of the shared memory segment.
    //
    int status;
    struct stat stats;
    status = fstat ( fd, &stats );
    if ( status == -1 )
    {
        printf ( "Unable to get the size of the VSI core data store[%s] errno: "
                 "%u[%m].\n", SHARED_MEMORY_SEGMENT_NAME, errno );
        (void) close ( fd );
        return 0;
    }
    //
    //  If the shared memory segment was just created, go initialize it.
    //
    vsi_core_handle vsiCore = 0;

    if ( stats.st_size <= 0 )
    {
        printf ( "VSI core data store[%s]\n"
                 "  is uninitialized - Initializing it...\n",
                 SHARED_MEMORY_SEGMENT_NAME );
        //
        //  Go initialize the shared memory segment.
        //
        vsiCore = sm_initialize ( fd, TOTAL_SHARED_MEMORY_SIZE );
        if ( vsiCore == 0 )
        {
            printf ( "Unable to initialize the VSI core data store[%s] errno: "
                     "%u[%m].\n", SHARED_MEMORY_SEGMENT_NAME, errno );
            (void) close ( fd );
            return 0;
        }
    }
    //
    //  If the shared memory segment has already been initialized then just
    //  map it into our address space.
    //
    else
    {
        //
        //  Map the shared memory file into virtual memory.
        //
        vsiCore = mmap ( NULL, sizeof(sharedMemory_t), PROT_READ|PROT_WRITE,
                         MAP_SHARED, fd, 0 );
        if ( vsiCore == MAP_FAILED )
        {
            printf ( "Unable to map the VSI core data store. errno: %u[%m].\n",
                     errno );
            return 0;
        }
    }
    printf ( "VSI core data store has been mapped into memory\n" );

    //
    //  Close the shared memory pseudo file as we don't need it any more.
    //
    (void) close ( fd );

    //
    //  Return the address of the memory mapped shared memory segment to the
    //  caller.
    //
    return (vsi_core_handle)vsiCore;
}


/*!-----------------------------------------------------------------------

    v s i C o r e C l o s e

    @brief Close the shared memory segment.

    This function will unmap the shared memory segment from memory, making it
    no longer accessable.  Note that this call will cause the data that has
    been modified in memory to be synchronized to the disk file that stores
    the data persistently.

    @param[in] handle - The handle to the VSI core data store

    @return None

------------------------------------------------------------------------*/
void vsi_core_close ( vsi_core_handle handle )
{
    (void) munmap ( handle, TOTAL_SHARED_MEMORY_SIZE );
}


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ i n s e r t

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
    @param[in] key - The key value associated with this message.
    @param[in] domain - The domain associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

    @return None

------------------------------------------------------------------------*/
void vsi_core_insert ( vsi_core_handle handle, offset_t key,
                       enum domains domain, unsigned long newMessageSize,
                       void* body )
{
    LOG ( "Inserting domain[%d] key[%lu]\n", domain, key );

    //
    //  Define the local message offset and pointer variables.
    //
    offset_t head;
    offset_t tail;

    sharedMessage_t* tailPtr;
    sharedMessage_t* newMessagePtr;

    offset_t newMessage;
    offset_t newMessageEnd;

    // HEX_DUMP_T ( body, newMessageSize, "New Message" );

    //
    //  Compute the message hash value from the input key.
    //
    unsigned long messageHash = sm_getHash ( key );

    //
    //  Get the address of the hash bucket that we will need for the input
    //  key.
    //
    hashBucket_t* hashBucket = sm_getBucketAddress ( handle, messageHash );

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
    dumpSemaphore ( "     ", &hashBucket->semaphore );

    semaphorePost ( &hashBucket->semaphore );

    LOG ( "%'lu  After Insert/semaphore post:\n", getIntervalTime() );
    dumpSemaphore ( "     ", &hashBucket->semaphore );

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

    r e m o v e M e s s a g e

    @brief Remove the "current" message from the message list.

    This function will remove the message at the specified offset from the
    message list for the specified hash bucket.

    WARNING: This function assumes that the hash bucket lock has been
    acquired before this function is called.

    @param[in] hashBucket - The address of the hash bucket to operate on.
    @param[in] currentMessage - The offset of the message to remove.
    @param[in] previousMessage - The offset of the previous message.

    @return 0 - Message successfully removed.

------------------------------------------------------------------------*/
static int removeMessage ( hashBucket_t* hashBucket, offset_t currentMessage,
                           offset_t previousMessage )
{
    sharedMessage_t* currentPtr;
    sharedMessage_t* previousPtr;

    LOG ( "%'lu  Removing message...\n", getIntervalTime() );

    //
    //  If this is the first message in the list...
    //
    currentPtr = hb_getAddress ( hashBucket, currentMessage );
    if ( currentMessage == hashBucket->head )
    {
        //
        //  Just make the head offset be the offset of the next message after
        //  the one we are removing.
        //
        hashBucket->head = currentPtr->nextMessageOffset;
    }
    //
    //  Otherwise, if this is not the first message in the list...
    //
    else
    {
        //
        //  Make the "next" offset in the previous message be the "next"
        //  offset in the message we are going to remove.
        //
        previousPtr = hb_getAddress ( hashBucket, previousMessage );
        previousPtr->nextMessageOffset = currentPtr->nextMessageOffset;
    }
    //
    //  Decrement the count of the number of messages in this list.
    //
    --hashBucket->currentMessageCount;

    //
    //  Decrement the count of the number of messages in the list within the
    //  semaphore.
    //
    --hashBucket->semaphore.messageCount;

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ f e t c h _ w a i t

    @brief Retrieve a message from the message buffer - Wait if empty.

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

    @param[in] handle - The base address of the shared memory segment.
    @param[in] key - The key value of the message to be removed.
    @param[in] domain - The domain value of the message to be removed.
    @param[in] messageSize - The number of bytes of data to be copied.
    @param[out] body - The address of where to put the message data.

    @return 0 if successful.
    @return not 0 on error which will be an errno value.

------------------------------------------------------------------------*/
int vsi_core_fetch_wait ( vsi_core_handle handle, offset_t key,
                          enum domains domain, unsigned long messageSize,
                          void* body )
{
    //
    //  Define the local message offset and pointer variables.
    //
    offset_t         currentMessage;
    offset_t         previousMessage;

    sharedMessage_t* messagePtr;

    int              transferSize;
    int              status = 0;

    //
    //  Compute the message hash value from the input key.
    //
    unsigned long messageHash = sm_getHash ( key );

    //
    //  Get the address of the hash bucket that we will need for the input
    //  key.
    //
    hashBucket_t* hashBucket = sm_getBucketAddress ( handle, messageHash );

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
    dumpSemaphore ( "  ", &hashBucket->semaphore );

    semaphoreWait ( &hashBucket->semaphore );

    --hashBucket->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    dumpSemaphore ( "  ", &hashBucket->semaphore );

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
            transferSize = messageSize <= messagePtr->messageSize
                           ? messageSize : messagePtr->messageSize;

            memcpy ( body, messagePtr->data, transferSize );

            //
            //  If no one else is now waiting for this message, go remove this
            //  message from the message list.
            //
            if ( hashBucket->semaphore.waiterCount <= 0 )
            {
                status = removeMessage ( hashBucket, currentMessage,
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
    dumpSemaphore ( "     ", &hashBucket->semaphore );

    //
    //  If we did not find the message we were looking for, return an error
    //  code to the caller.
    //
    if ( currentMessage == END_OF_BUCKET_DATA )
    {
        status = ENOMSG;
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


int vsi_core_fetch ( vsi_core_handle handle, offset_t key,
                     enum domains domain, unsigned long newMessageSize,
                     void* body )
{
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
