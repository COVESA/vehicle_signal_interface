/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    s i g n a l s . c

    This file implements the VSI signal functions.

    Note: See the signals.h header file for a detailed description of each of the
    functions implemented here.

	TODO: All the references to "semaphore" should really be "condition variable"!

-----------------------------------------------------------------------------*/
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "vsi.h"
#include "signals.h"
#include "vsi_core_api.h"

// #define VSI_DEBUG
// #undef LOG
// #define LOG printf

//
//	Define the externally defined functions.
//
extern unsigned long getIntervalTime ( void );
extern void          dumpSemaphore   ( semaphore_t* semaphore );


/*!-----------------------------------------------------------------------

    H e l p e r   M a c r o s

    The following are compiler macros that are useful in the following code.

    Helper macro to get a signal ID from a specified name. If the signal could
    not be found, an error will be returned. Rather than duplicate this logic
    across every function, define a macro for simplicity.

    This macro will trigger a return with an error code if one is returned
    from vsi_name_string_to_id. An error is detected by checking if the result
    of the call is non-zero.

------------------------------------------------------------------------*/
#define VSI_LOOKUP_RESULT_NAME( result )                      \
{                                                             \
    if ( ( ! result->name ) || ( ! result->domainId ) )       \
    {                                                         \
        return EINVAL;                                        \
    }                                                         \
    int status = vsi_name_string_to_id ( result->domainId,    \
                                         result->name,        \
                                         &result->signalId ); \
    if ( status )                                             \
    {                                                         \
        return EINVAL;                                        \
    }                                                         \
}


/*
    Helper macro strictly for checking if input arguments are valid. This
    should be used for brevity. The expected usage is to pass in the
    conditions expected to be true and this macro will error if any of them
    are not.
*/
#define CHECK_AND_RETURN_IF_ERROR(input) \
{                                        \
    if ( ! input )                       \
    {                                    \
        return EINVAL;                   \
    }                                    \
}


#if 0
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
    printf ( "WARNING: semaphoreCleanupHandler was called!\n" );

    signal_list* signalList = arg;

    pthread_mutex_unlock ( &signalList->semaphore.mutex );
}


/*!-----------------------------------------------------------------------

    S L _ L O C K

    @brief Acquire the signal list control structure lock.

    This function will hang if the lock is currently not available and return
    when the lock has been successfully acquired.  The address of the signal
    list control block is required as the function argument.

    NOTE: It is REQUIRED that the "push" and "pop" cleanup functions be
    executed in the same lexical nesting level in the same function so the
    "lock" and "unlock" macros must be used at the same lexical nesting level
    and in the same function as well.  These functions also had to be
    implemented as macros instead of functions because of the lexical level
    limitation.

------------------------------------------------------------------------*/
#define SL_LOCK(sigList)                                                      \
    if ( ( status = pthread_mutex_lock ( &sigList->semaphore.mutex ) ) != 0 ) \
    {                                                                         \
        printf ( "Error: Unable to acquire the signal list lock: "            \
                 "%p - %d[%s]\n", sigList, status, strerror(status) );        \
    }                                                                         \
    pthread_cleanup_push ( semaphoreCleanupHandler, sigList );


/*!-----------------------------------------------------------------------

    S L _ U N L O C K

    @brief Release the signal list control structure lock.

    This function will unlock the mutex on the signal list.  If another
    process or thread is waiting on this lock, the scheduler will decide which
    will run so the order of processes/threads may not be the same as the
    order in which they called the lock function.

------------------------------------------------------------------------*/
#define SL_UNLOCK(sigList)                              \
    pthread_cleanup_pop ( 0 );                          \
    pthread_mutex_unlock ( &sigList->semaphore.mutex );
#endif


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G e n e r a t i o n
    =========================================

    TODO: All of these API functions are too simple - Eliminate them and just
    put the "real" logic in their place.


    v s i _ i n s e r t _ s i g n a l

    Fire a vehicle signal by its ID.

------------------------------------------------------------------------*/
int vsi_insert_signal ( vsi_result* result )
{
    LOG ( "vsi_insert_signal called with domain: %d, signal: %d, name: [%s]\n",
          result->domainId, result->signalId, result->name );

    CHECK_AND_RETURN_IF_ERROR ( result && result->data && result->dataLength );

    vsi_core_insert ( result->domainId, result->signalId,
                      result->dataLength, &result->data );
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ i n s e r t _ s i g n a l _ b y _ n a m e

    Fire a vehicle signal by its name.

------------------------------------------------------------------------*/
int vsi_insert_signal_by_name ( vsi_result* result )
{
    LOG ( "vsi_insert_signal_by_name called with domain: %d, signal: %d, "
          "name: [%s]\n", result->domainId, result->signalId, result->name );

    VSI_LOOKUP_RESULT_NAME ( result );

    return vsi_insert_signal ( result );
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ s i g n a l

    Fetch the oldest entry in the core database for a signal by ID.

------------------------------------------------------------------------*/
int vsi_get_oldest_signal ( vsi_result* result )
{
    LOG ( "vsi_get_oldest_signal: %d,%d[%s]\n", result->domainId,
          result->signalId, (char*)( result->name ) );

    CHECK_AND_RETURN_IF_ERROR ( result && result->data && result->dataLength );

    result->status = vsi_core_fetch ( result->domainId, result->signalId,
                                      &result->dataLength, result->data );

    LOG ( "vsi_get_oldest_signal returning: %lu\n", *(unsigned long*)result->data );

    return result->status;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ s i g n a l _ b y _ n a m e

    Fetch the oldest entry in the core database for a signal by it's name.

------------------------------------------------------------------------*/
int vsi_get_oldest_signal_by_name ( vsi_result* result )
{
    VSI_LOOKUP_RESULT_NAME ( result );

    return vsi_get_oldest_signal ( result );
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ s i g n a l

    Fetch the latest value of the specified signal by ID.

------------------------------------------------------------------------*/
int vsi_get_newest_signal ( vsi_result* result )
{
    CHECK_AND_RETURN_IF_ERROR ( result && result->data && result->dataLength );

    result->status = vsi_core_fetch_newest ( result->domainId, result->signalId,
                                             &result->dataLength, result->data );
    return result->status;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ s i g n a l _ b y _ n a m e

    Fetch the latest value of the specified signal by name.

------------------------------------------------------------------------*/
int vsi_get_newest_signal_by_name ( vsi_result* result )
{
    VSI_LOOKUP_RESULT_NAME ( result );

    return vsi_get_newest_signal ( result );
}


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l

    Flush all data for a signal by domain and ID.

------------------------------------------------------------------------*/
int vsi_flush_signal ( const domain_t domainId, const signal_t signalId )
{
    return vsi_core_flush_signal ( domainId, signalId );
}


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l _ b y _ n a m e

    Flush all data for a signal by name.

------------------------------------------------------------------------*/
int vsi_flush_signal_by_name ( const domain_t domain, const char* name )
{
    domain_t domainId = domain;
    signal_t signalId = 0;

    CHECK_AND_RETURN_IF_ERROR ( name );

    vsi_name_string_to_id ( domain, name, &signalId );

    return vsi_flush_signal ( domainId, signalId );
}


/*!----------------------------------------------------------------------------

    f i n d S i g n a l L i s t

    @brief Find the the signal list for a domain and signal value.

    This function will search the signals btree for a match to the domain and
    id value supplied.  If a match is found, the address of the signal list
    control block will be returned to the caller.  If no match is found, a new
    signal list control block will be created and inserted into the btree
    database.  In either case, the signal list control address will be
    returned to the caller.

    The caller is responsible for populating the privateId and name for this
    signal if it is a new one and updating the appropriate indices for those
    fields.

    @param[in] - domain - The domain of the signal list to find
    @param[in] - signal - The id of the signal list to find

    @return If successful, the address of the signal list control block
            If no memory is availabe for a new list, a NULL

-----------------------------------------------------------------------------*/
signal_list* findSignalList ( domain_t domain, signal_t signal )
{
    signal_list  requestedSignal;
    signal_list* signalList;
    int          status = 0;

    //
    //  Initialize the fields we will need to find the requested signal list.
    //
    requestedSignal.domainId = domain;
    requestedSignal.signalId = signal;

    //
    //  Go find the requested signal list.
    //
    signalList = btree_search ( &vsiContext->signalIdIndex, &requestedSignal );

    //
    //  If we didn't find this signal list control block then this is the
    //  first time this domain/id have been seen so we need to create a new
    //  signal list control block for it.
    //
    if ( signalList == NULL )
    {
        //
        //  Go allocate a new signal list control block in the shared memory
        //  segment.
        //
        signalList = sm_malloc ( SIGNAL_LIST_SIZE );

        //
        //  If the allocation failed, we have exceeded the amount of memory in
        //  the shared memory segment.  This is a fatal condition so let the
        //  user know and quit!
        //
        if ( signalList == NULL )
        {
            printf ( "Error: Unable to allocate a new signal list - "
                     "Shared memory segment is full!\n" );
            return 0;
        }
        //
        //  Initialize and populate the new signal list control block.
        //
        signalList->domainId           = domain;
        signalList->signalId           = signal;
        signalList->privateId          = 0;
        signalList->name               = 0;
        signalList->currentSignalCount = 0;
        signalList->totalSignalSize    = 0;
        signalList->head               = END_OF_LIST_MARKER;
        signalList->tail               = END_OF_LIST_MARKER;

        //
        //  Initialize the signal list mutex and condition variable.
        //
        status = pthread_mutex_init ( &signalList->semaphore.mutex,
                                      &smControl->masterMutexAttributes );
        if ( status != 0 )
        {
            printf ( "Unable to initialize signal list mutex - errno: %u[%m].\n",
                     status );
            sm_free ( signalList );
            return 0;
        }
        status = pthread_cond_init ( &signalList->semaphore.conditionVariable,
                                     &smControl->masterCvAttributes );
        if ( status != 0 )
        {
            printf ( "Unable to initialize signal list condition variable - "
                     "errno: %u[%m].\n", status );
            sm_free ( signalList );
            return 0;
        }
        //
        //  Initialize the semaphore counters for this signal list control
        //  block.
        //
        signalList->semaphore.messageCount = 0;
        signalList->semaphore.waiterCount  = 0;

        //
        //  Insert the new signal list control block into the btree.
        //
        btree_insert ( &vsiContext->signalIdIndex, signalList );
    }
    //
    //  Return the requested signal list to the caller.
    //
    return signalList;
}


/*!-----------------------------------------------------------------------

    s m _ i n s e r t

    @brief Insert a new signal into the signal list.

    This function will find the appropriate signal list for the given signal
    and domain and insert the caller's data into the message list.  New
    messages are inserted at the end of the list so the oldest messages are at
    the beginning of the list.

    The "signal" is an arbitrary integer that uniquely identifies the message
    type within the domain of the message.  Domains are an identifier that
    specifies a class of messages.  For instance, CAN messages can all be
    considered to be messages in the CAN domain.  Within that domain, the
    message ID field uniquely identifies the type of message.

    The "body" pointer supplied by the caller must point to the beginning of
    the data that makes up the message we want to store.  The message size
    specifies the number of bytes that will be read from the user's pointer
    and copied into the shared memory segment.

    @param[in] domain - The domain associated with this message.
    @param[in] signal - The signal value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

    @return 0 if successful
            Otherwise the error code

------------------------------------------------------------------------*/
int sm_insert ( domain_t domain, signal_t signal, unsigned long newMessageSize,
                void* body )
{
    int status = 0;

    LOG ( "Inserting domain[%d] signal[%d]\n", domain, signal );

    HX_DUMP ( body, newMessageSize, "New Message" );

    //
    //  Go find the signal list control block for this domain and signal.  Note
    //  that if the signal list does not exist yet, a new one will be created
    //  and initialized.
    //
    signal_list* signalList = findSignalList ( domain, signal );

    //
    //  Now we need to create a new entry for this message list so compute the
    //  size that we will need and allocate that much memory in the shared
    //  memory segment.
    //
    int signalDataSize = newMessageSize + SIGNAL_DATA_HEADER_SIZE;
    signal_data* signalData = sm_malloc ( signalDataSize );
    if ( signalData == NULL )
    {
        printf ( "Error: Unable to allocate a new signal list - "
                 "Shared memory segment is full!\n" );
        return ENOMEM;
    }
    //
    //  Acquire the lock on this signal list.
    //
    //  Note that this call will hang if someone else is currently using this
    //  signal list.  It will return once the lock is acquired and it is safe
    //  to manipulate the signal list.
    //
    // SL_LOCK ( signalList );

    //
    //  Initialize all of the fields in the message header of the new message
    //  that we are inserting.  We will be inserting this new message at the
    //  end of the message list which is where the "tail" pointer is pointing.
    //
    //  If the message list is currently empty (because the head pointer is
    //  NULL), make the head pointer point to our new message.
    //
    offset_t newMessageOffset = toOffset ( signalData );
    if ( signalList->head == END_OF_LIST_MARKER )
    {
        signalList->head = newMessageOffset;
    }
    //
    //  If the tail pointer is not NULL, make the current tail message point
    //  to our new message.
    //
    if ( signalList->tail != END_OF_LIST_MARKER )
    {
        ((signal_data*)toAddress(signalList->tail))->nextMessageOffset =
            newMessageOffset;
    }
    //
    //  Now make the tail pointer point to our new message.
    //
    signalList->tail = newMessageOffset;

    //
    //  Make the "next" pointer in our new message indicate the "end" of the
    //  list.
    //
    signalData->nextMessageOffset = END_OF_LIST_MARKER;

    //
    //  Copy the message body into the message list.
    //
    //  TODO: Validate that the message size is "reasonable"!
    //
    signalData->messageSize = newMessageSize;
    memcpy ( (void*)(&signalData->data), body, newMessageSize );

    //
    //  Increment the signal count and total message data size.
    //
    ++signalList->currentSignalCount;
    signalList->totalSignalSize += newMessageSize;

    //
    //  Post to the semaphore to reflect the message we just inserted into the
    //  message list.
    //
    //  Note that doing this post may result in a different process and/or
    //  thread running before the call comes back here.
    //
    LOG ( "%'lu  Before Insert/semaphore post with sem: %p[%lu]\n",
          getIntervalTime(), &signalList->semaphore,
          toOffset ( &signalList->semaphore ) );

    SEM_DUMP ( &signalList->semaphore );

    ++signalList->semaphore.messageCount;

    semaphorePost ( &signalList->semaphore );

    LOG ( "%'lu  After Insert/semaphore post:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    //
    //  Give up the signal list lock.
    //
    // SL_UNLOCK ( signalList );

    //
    //  Return the status indicator to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ r e m o v e S i g n a l

    @brief Remove the oldest signal from the signal list.

    This function will remove the signal at the beginning of the specified
    signal list.  This will be the oldest message in the list.

    @param[in] signalList - The address of the signal list to operate on.

    @return 0 - Message successfully removed.

------------------------------------------------------------------------*/
int sm_removeSignal ( signal_list* signalList )
{
    signal_data* signalData;

    LOG ( "%'lu  Removing signal with %d - %d\n", getIntervalTime(),
          signalList->domainId, signalList->signalId );

    //
    //  If this signal list is empty, just return without doing anything.
    //
    if ( signalList->head == END_OF_LIST_MARKER )
    {
        return 0;
    }
    //
    //  Grab the first signal on the given signalList control block.
    //
    signalData = toAddress ( signalList->head );

    //
    //  If there is only one message in the current list then the head and
    //  tail offsets will be the same and we should just set both of them to
    //  the "empty" list state.
    //
    if ( signalList->currentSignalCount == 1 )
    {
        signalList->head = END_OF_LIST_MARKER;
        signalList->tail = END_OF_LIST_MARKER;
    }
    //
    //  Otherwise, just make the head offset be the offset of the next signal
    //  after the one we are removing.
    //
    else
    {
        signalList->head = signalData->nextMessageOffset;
    }
    //
    //  Decrement the count of the number of signals in this list and the
    //  total amount of space occupied by those signals.
    //
    --signalList->currentSignalCount;
    signalList->totalSignalSize -= signalData->messageSize;

    //
    //  Free up the shared memory occupied by this signal data structure.
    //
    sm_free ( signalData );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    s m _ f e t c h

    @brief Retrieve a signal from the signal buffer.

    This function will attempt to retrieve the oldest signal with the
    specified signal and domain from the shared memory signal buffer.  The
    signal that was found (and returned to the caller) will be removed from
    the signal list.  If there are no signal that matches the user's request,
    this function will hang (on a semaphore) until an appropriate signal is
    available before returning to the caller if requested.

    If the signal list is not empty, the head of the list will be returned to
    the caller.  This is also the oldest signal in the list since the signals
    are added at the end of the list.

    When calling this function, the bodySize should be the size of the data
    buffer supplied as the "body" argument.  When we copy the data from the
    shared memory segment to the "body" buffer, the smaller of either the body
    buffer size or the number of bytes of data in the signal found will be
    copied into the body buffer.  The number of bytes actually copied will be
    returned to the caller in the bodySize parameter.

    Note that since this function does not alter the structure of the signal
    list we don't need to lock the signal list semaphore during the processing
    (except when we wait on an empty list).  We do lock the list of signal
    instance records attached to the signalList however as we will be
    modifying that list.

    @param[in]  domain - The domain value of the signal to be removed.
    @param[in]  signal - The signal ID value of the signal to be removed.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the signal data.
    @param[in]  wait - If true, wait for data if domain/signal is not found.

    @return 0 if successful.
            ENODATA - If waitForData == false and domain/signal is not found.
            any other value is an errno value.

    TODO: Can we combine this function with sm_fetch_newest?
------------------------------------------------------------------------*/
int sm_fetch ( domain_t domain, signal_t signal, unsigned long* bodySize,
               void* body, bool wait )
{
    signal_data* signalData;
    int          transferSize;
    int          status = 0;

    LOG ( "Fetching signal domain[%d], signal[%d], bodySize[%p-%lu], "
          "body[%p], wait[%d]\n", domain, signal, bodySize, *bodySize,
          body, wait );
    //
    //  Go find the signal list control block for this domain and signal.
    //
    signal_list* signalList = findSignalList ( domain, signal );

    //
    //  If we didn't find this signal list control block then we need to just
    //  return an error code to the caller.  This should only happen if we run
    //  out of memory.
    //
    //  If this signal list was found but it is empty and we don't want to
    //  wait for the data to arrive then just return the "no data" error code
    //  to the caller and quit.
    //
    if ( !signalList || ( ( signalList->currentSignalCount == 0 ) && !wait ) )
    {
        LOG ( "Warning: Signal list for domain %d, signal %d was not found "
              "or empty.\n", domain, signal );
        return ENODATA;
    }
    //
    //  Go wait if there are no signals in the signal list to be read.
    //
    //  Note that we will increment the "waiter" count during the time that we
    //  are potentially waiting on the semaphore and decrement it again when
    //  we come back from the wait.  This is used to tell the "fetch" logic
    //  when it is OK to delete the signal that is being waited on.  If more
    //  than one process is waiting on this signal then we want to give the
    //  signal to all of the processes that are waiting before we delete the
    //  signal from the signal list.
    //
    // SL_LOCK ( signalList );

    ++signalList->semaphore.waiterCount;

    LOG ( "%'lu  Before Fetch/semaphore wait sem: %p[%lu]\n",
          getIntervalTime(), &signalList->semaphore,
          toOffset ( &signalList->semaphore ) );

    SEM_DUMP ( &signalList->semaphore );

    semaphoreWait ( &signalList->semaphore );

    --signalList->semaphore.messageCount;

    --signalList->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    //
    //  When we come back from the wait above, there should be some signal
    //  data to read.
    //
    //  Get the actual memory pointer to the current signal.
    //
    //  TODO: Check to make sure the head is not END.
    //
    signalData = toAddress ( signalList->head );

    //
    //  Copy the signal body into the caller supplied buffer.
    //
    //  Note that we will copy the SMALLER of either the signal size supplied
    //  by the caller or the actual data size in the signal that was found.
    //
    transferSize = *bodySize <= signalData->messageSize
                   ? *bodySize
                   : signalData->messageSize;

    memcpy ( body, signalData->data, transferSize );

    //
    //  Return the number of bytes that were copied to the caller.
    //
    *bodySize = transferSize;

    //
    //  If no one else is now waiting for this signal, go remove this signal
    //  from the signal list.
    //
    if ( signalList->semaphore.waiterCount <= 0 )
    {
        status = sm_removeSignal ( signalList );
    }
    LOG ( "%'lu  At the end of Fetch - waiterCount: %d\n", getIntervalTime(),
          signalList->semaphore.waiterCount );

    SEM_DUMP ( &signalList->semaphore );

    //
    //  Unlock the condition variable for this signal.
    //
    // SL_UNLOCK ( signalList );

    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ f e t c h _ n e w e s t

    @brief Retrieve the newest signal from the signal list.

    This function will attempt to retrieve the newest signal with the
    specified signal and domain from the shared memory signal list.  The
    signal that is found (and returned to the caller) will NOT be removed from
    the signal list.  If there are no signals that match the user's request,
    this function will hang (on a semaphore) until an appropriate signal is
    available before returning to the caller depending on the setting of the
    "wait" input parameter.

    When calling this function, the bodySize should be the size of the data
    buffer supplied as the "body" argument.  When we copy the data from the
    shared memory segment to the "body" buffer, the smaller of either the body
    buffer size or the number of bytes of data in the signal found will be
    copied into the body buffer.  The actual number of data bytes copied will
    be returned to the caller in the bodySize variable.

    Note that since this function does not alter the structure of the signal
    list we don't need to lock the signal list semaphore during the processing
    (except when we wait on an empty list).

    @param[in]  domain - The domain value of the signal to be removed.
    @param[in]  signal - The signal value of the signal to be removed.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the signal data.
    @param[in]  wait - If true, wait for data if domain/signal is not found.

    @return 0 if successful.
            ENODATA - If waitForData == true and domain/signal is not found.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_fetch_newest ( domain_t domain, signal_t signal, unsigned long* bodySize,
                      void* body, bool wait )
{
    //
    //  Define the local signal offset and pointer variables.
    //
    signal_list* signalList;
    signal_data* signalData;
    int          transferSize = 0;
    int          status = 0;

    LOG ( "Fetching newest signal domain[%d], signal[%d], wait[%d]\n", domain,
          signal, wait );
    //
    //  Go find the signal list control block for this domain and signal.
    //
    signalList = findSignalList ( domain, signal );

    //
    //  If this signal list is empty and we don't want to wait for the data to
    //  arrive then just return the "no data" error code to the caller and
    //  quit.
    //
    if ( !signalList || ( signalList->currentSignalCount == 0 && !wait ) )
    {
        LOG ( "Signal list is empty - Returning error code 61 - ENODATA\n" );
        return ENODATA;
    }
    //
    //  Go wait if there are no messages in the signal list to be read.
    //
    //  Note that we will increment the "waiter" count during the time that we
    //  are potentially waiting on the semaphore and decrement it again when
    //  we come back from the wait.  This is used to tell the "fetch" logic
    //  when it is OK to delete the signal that is being waited on.  If more
    //  than one process is waiting on this signal then we want to give the
    //  signal to all of the processes that are waiting before we delete the
    //  signal from the signal list.
    //
    // SL_LOCK ( signalList );

    ++signalList->semaphore.waiterCount;

    LOG ( "%'lu  Before Fetch/semaphore wait sem: %p[%lu]\n",
          getIntervalTime(), &signalList->semaphore,
          toOffset ( &signalList->semaphore ) );

    SEM_DUMP ( &signalList->semaphore );

    semaphoreWait ( &signalList->semaphore );

    --signalList->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    //
    //  If we get here, there is at least one signal in this signal list.  At
    //  that point, we can take a look at the last signal in the list.
    //
    //  TODO: Check to make sure the tail is not END.
    //
    signalData = toAddress ( signalList->tail );

    //
    //  Copy the signal body into the caller supplied buffer.
    //
    //  Note that we will copy the SMALLER of either the signal size
    //  supplied by the caller or the actual data size in the signal
    //  that was found.
    //
    //  TODO: Make sure the requested size is "reasonable".
    //
    transferSize = *bodySize <= signalData->messageSize
                   ? *bodySize : signalData->messageSize;

    memcpy ( body, signalData->data, transferSize );

    //
    //  Return the number of bytes that were copied to the caller.
    //
    *bodySize = transferSize;

    //
    //  Return to the caller with a good completion code.
    //
    LOG ( "%'lu  At the end of Fetch - waiterCount: %d\n", getIntervalTime(),
          signalList->semaphore.waiterCount );

    SEM_DUMP ( &signalList->semaphore );

    //
    //  Unlock the condition variable for this signal list.
    //
    // SL_UNLOCK ( signalList );

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
    domain and signal value and remove them from the data store.

    @param[in]  domain - The domain value of the message to be removed.
    @param[in]  signal - The signal value of the message to be removed.

    @return 0 if successful.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_flush_signal ( domain_t domain, signal_t signal )
{
    //
    //  Define the local signal offset and pointer variables.
    //
    signal_data* signalData;
    offset_t     signalDataOffset;
    int          status = 0;

    LOG ( "Flushing signal domain[%d], signal[%d]\n", domain, signal );

    //
    //  Go find the signal list control block for this domain and signal.
    //
    signal_list* signalList = findSignalList ( domain, signal );

    //
    //  If this signal list doesn't exist or is empty then we don't need to do
    //  anything.  In this case, just return a good completion code.
    //
    if ( signalList == NULL || signalList->currentSignalCount == 0 )
    {
        return 0;
    }
    //
    //  Acquire the lock on this signal list.
    //
    //  Note that this call will hang if someone else is currently using this
    //  signal list.  It will return once the lock is acquired and it is safe
    //  to manipulate the signal list.
    //
    // SL_LOCK ( signalList );

    //
    //  Get the head of the signal list.
    //
    signalDataOffset = signalList->head;

    //
    //  Repeat until we reach the end of the signal list in this bucket.
    //
    while ( signalDataOffset != END_OF_LIST_MARKER )
    {
        //
        //  Get the actual memory pointer to the current signal data.
        //
        signalData =  toAddress ( signalDataOffset );

        //
        //  Save the offset of the next signal before we destroy the current
        //  signal.
        //
        signalDataOffset = signalData->nextMessageOffset;

        //
        //  Go free the memory occupied by this signal record.
        //
        sm_free ( signalData );

        //
        //  Increment our signal offsets to the next signal in the list.
        //
        signalData = toAddress ( signalDataOffset );
    }
    //
    //  When we get here, we've removed all of the signal data records from
    //  this signal list so set everything in the signal list to it's "empty"
    //  state.
    //
    signalList->head               = END_OF_LIST_MARKER;
    signalList->tail               = END_OF_LIST_MARKER;
    signalList->currentSignalCount = 0;
    signalList->totalSignalSize    = 0;

    //
    //  If anyone is waiting for this signal, go release them to search the
    //  signal list again.
    //
    //  Note that this call may not return immediately if executing the post
    //  resulted in another thread/process running.
    //
    signalList->semaphore.messageCount = 0;
    semaphorePost ( &signalList->semaphore );

    //
    //  Give up the signal list lock.
    //
    // SL_UNLOCK ( signalList );

    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G r o u p   F u n c t i o n s
    ===================================================

    g r o u p E x i s t s

    This is a helper function that determines if the specified group currently
    exists in the group btree.  This function is only used by the internal VSI
    API functions.

------------------------------------------------------------------------*/
static bool groupExists ( const group_t groupId )
{
    vsi_signal_group  requestedSignalGroup = { 0 };
    vsi_signal_group* signalGroup;

    CHECK_AND_RETURN_IF_ERROR ( groupId );

    requestedSignalGroup.groupId = groupId;

    signalGroup = btree_search ( &vsiContext->groupIdIndex, &requestedSignalGroup );

    if ( signalGroup != NULL )
    {
        return true;
    }
    return false;
}


/*!-----------------------------------------------------------------------

    v s i _ c r e a t e _ s i g n a l _ g r o u p

    @brief Create a new signal group.

------------------------------------------------------------------------*/
int vsi_create_signal_group ( const group_t groupId )
{
    int status = 0;

    LOG ( "Creating group %d\n", groupId );

    CHECK_AND_RETURN_IF_ERROR ( groupId );

    if ( groupExists ( groupId ) )
    {
        LOG ( "Error: group[%d] already exists\n", groupId );
        return EINVAL;
    }
    vsi_signal_group* signalGroup = sm_malloc ( sizeof(vsi_signal_group) );

    if ( signalGroup == NULL )
    {
        printf ( "ERROR: Unable to allocate shared memory for group\n" );
        return ENOMEM;
    }
    //
    //  Initialize the rest of the fields in the signal group data structure.
    //
    signalGroup->groupId = groupId;
    signalGroup->count   = 0;
    signalGroup->head    = 0;
    signalGroup->tail    = 0;

	//
	//  Initialize the group record mutex.
	//
	status = pthread_mutex_init ( &signalGroup->mutex,
								  &smControl->masterMutexAttributes );
	if ( status != 0 )
	{
		printf ( "Unable to initialize the group mutex - errno: %u[%m].\n",
				 status );
		sm_free ( signalGroup );
		return status;
	}

    LOG ( "  inserting into btree\n" );

    status = btree_insert ( &vsiContext->groupIdIndex, signalGroup );

    LOG ( "  returning %d\n", status );

    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ d e l e t e _ s i g n a l _ g r o u p

    @brief Delete an existing signal group.

    Delete a signal group.  This function will delete the specified signal
    group and release any shared memory occupied by it's members.  If there
    are any signal lists associated with the group, they will not be deleted.

------------------------------------------------------------------------*/
int vsi_delete_signal_group ( const group_t groupId )
{
    int status = 0;

    CHECK_AND_RETURN_IF_ERROR ( groupId );

    //
    //  Get the group object that the user specified.
    //
    vsi_signal_group  requestedGroup = { 0 };
    vsi_signal_group* signalGroup;

    requestedGroup.groupId = groupId;

    signalGroup = btree_search ( &vsiContext->groupIdIndex, &requestedGroup );

    //
    //  If the specified group does not exist, return an error code to the
    //  caller.
    //
    if ( signalGroup == NULL )
    {
        return EINVAL;
    }
    //
    //  Get the offset of the beginning of the signal list.
    //
    offset_t groupDataOffset = signalGroup->head;

    //
    //  While we have not reached the end of the signal list for this group...
    //
	vsi_signal_group_data* groupData;
#ifdef VSI_DEBUG
	signal_list*           signalList;
#endif

    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        //
        //  Get an actual pointer to this group data record.
        //
        groupData = toAddress ( groupDataOffset );

        //
        //  Now get the address of the signal list structure.
        //

#ifdef VSI_DEBUG

        signalList = toAddress ( groupData->signalList );
        //
        //  Go print the contents of the current signal structure.
        //
        LOG ( "%s      Removing... Domain: %d, Signal: %d, Name: %s\n", "",
              signalList->domainId, signalList->signalId,
              (char*)toAddress ( signalList->name ) );
#endif
        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;

        //
        //  Go give this signal structure back to the system.
        //
        sm_free ( groupData );
    }
    //
    //  Go remove this signal group object from the group definition btree.
    //
    status = btree_delete ( &vsiContext->groupIdIndex, signalGroup );

    //
    //  Give the signal group object back to the memory manager.
    //
    sm_free ( signalGroup );

    //
    //  Return the completion status back to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ f e t c h _ s i g n a l _ g r o u p

    @brief Delete an existing signal group.

    Delete a signal group.  This function will delete the specified signal
    group and release any shared memory occupied by it's members.  If there
    are any signal lists associated with the group, they will not be deleted.

------------------------------------------------------------------------*/
vsi_signal_group* vsi_fetch_signal_group ( const group_t groupId )
{
    LOG ( "Fetching signal group: %u\n", groupId );

    //
    //  Get the group object that the user specified.
    //
    vsi_signal_group  requestedGroup = { 0 };
    vsi_signal_group* signalGroup;

    requestedGroup.groupId = groupId;

    signalGroup = btree_search ( &vsiContext->groupIdIndex, &requestedGroup );

    LOG ( "  Returning signal group: %p\n", signalGroup );

    //
    //  Return the address of the signal group structure.  Note that this will
    //  return a NULL pointer if the group could not be found.
    //
    return signalGroup;
}


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p

    @brief Add a new signal to the specified group.

    This function will add a new signal to the specified group.  New signals
    will be added to the tail end of the group's signal list.

------------------------------------------------------------------------*/
int vsi_add_signal_to_group ( const domain_t domainId,
                              const signal_t signalId,
                              const group_t  groupId )
{
    vsi_signal_group* signalGroup = 0;

    //
    //  Go find the group record for this groupId.
    //
    signalGroup = vsi_fetch_signal_group ( groupId );

    //
    //  If the group record was not found, return an error code to the caller
    //  and quit.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }
    //
    //  Go find the signal list for this domain/signal.
    //
    signal_list* signalList = findSignalList ( domainId, signalId );

    //
    //  If the could not be found or created, return an error code to the
    //  caller.
    //
    if ( signalList == NULL )
    {
        return ENOMEM;
    }
    //
    //  Go get the memory for a new signal group data record.
    //
    vsi_signal_group_data* groupData = sm_malloc ( sizeof(vsi_signal_group_data) );

    //
    //  If the memory allocation failed, issue an error message and return an
    //  error code.
    //
    if ( groupData == NULL )
    {
        printf ( "Error: Unable to allocate shared memory for a new signal "
                 "group data record\n" );
        return ENOMEM;
    }
    //
    //  Fill in the fields of the new signal group data structure.
    //
    groupData->nextMessageOffset = END_OF_LIST_MARKER;
    groupData->signalList        = toOffset ( signalList );

    //
    //  Get the offset of the new signal group data structure.
    //
    offset_t newGroupDataOffset = toOffset ( groupData );

    //
    //  If this signal group is not empty, make the current tail record
    //  point to this new signal group data record.  We will insert this new
    //  record at the end of the list.
    //
    if ( signalGroup->count != 0 )
    {
        ((signal_data*)toAddress(signalGroup->tail))->nextMessageOffset =
            newGroupDataOffset;
    }
    //
    //  If the signal group is currently empty, make the head pointer point to
    //  our new signal group data record.
    //
    else
    {
        signalGroup->head = newGroupDataOffset;
    }
    //
    //  Now make the tail pointer point to our new message.
    //
    signalGroup->tail = newGroupDataOffset;

    //
    //  Increment the signal count for this group.
    //
    signalGroup->count++;

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p _ b y _ n a m e

    @brief Add a new signal to the specified group by name.

    This function will add a new signal to the specified group.

------------------------------------------------------------------------*/
int vsi_add_signal_to_group_by_name ( const domain_t domainId,
                                      const char*    name,
                                      const group_t  groupId )
{
    signal_t signalId;

    vsi_name_string_to_id ( domainId, name, &signalId );

    return vsi_add_signal_to_group ( domainId, signalId, groupId );
}


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p

    @brief Remove a signal from the specified group.

    This function will add a new signal to the specified group.

    TODO: This removal is awkward.  We need to search for the item we want and
    then call the remove function which will perform another search for the
    same item - Because we don't know what the "pointer" values is!  We need a
    better way to do this that is still fairly generic.

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group ( const domain_t domainId,
                                   const signal_t signalId,
                                   const group_t  groupId )
{
    vsi_signal_group*      signalGroup;
    vsi_signal_group_data* groupData;
    vsi_signal_group_data* trailingPtr;
    signal_list*           signalList;

    //
    //  Go find the group record for this groupId.
    //
    signalGroup = vsi_fetch_signal_group ( groupId );

    //
    //  If the group record was not found, return an error code to the caller
    //  and quit.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }
    //
    //  While we have not reached the last signal in the group list...
    //
    offset_t groupDataOffset = signalGroup->head;

    trailingPtr = 0;

    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        //
        //  Get a pointer to the signal group data structure.
        //
        groupData = toAddress ( groupDataOffset );

        //
        //  Get the pointer to the signal list in the group data structure.
        //
        signalList = toAddress ( groupData->signalList );

        //
        //  If this is the signal that we are looking for...
        //
        if ( signalList->domainId == domainId &&
             signalList->signalId == signalId )
        {
            //
            //  If we are at the beginning of the list then just fix the
            //  signal group head pointer to point to the next structure.
            //  Note that if this is the only structure in the list then the
            //  head pointer will be set to END_OF_LIST_MARKER which is what
            //  we want.
            //
            if ( trailingPtr == 0 )
            {
                signalGroup->head = groupData->nextMessageOffset;
            }
            else
            {
                trailingPtr->nextMessageOffset = groupData->nextMessageOffset;
            }
            //
            //  If the structure we want to delete is the last structure in
            //  the group list then reset the group head offset to be the
            //  trailing pointer offset value.
            //
            if ( groupData->nextMessageOffset == END_OF_LIST_MARKER )
            {
                if ( trailingPtr == 0 )
                {
                    signalGroup->tail = END_OF_LIST_MARKER;
                }
                else
                {
                    signalGroup->tail = toOffset ( trailingPtr );
                }
            }
            //
            //  Go return the structure we found to the shared memory free
            //  list.
            //
            sm_free ( groupData );

            //
            //  Decrement the number of records in this signal group.
            //
            signalGroup->count--;

            //
            //  Return a good completion code to the caller.
            //
            return 0;
        }
        //
        //  If this record is not the one we are looking for, save the current
        //  position as the "trailing pointer" and advance to the next record
        //  in the list.
        //
        trailingPtr = groupData;
        groupDataOffset = groupData->nextMessageOffset;
    }
    //
    //  If we get here, the specified domain/signal were not found in any of
    //  the signal structures associated with this group.  In this case,
    //  return a "not found" error to the caller.
    //
    return ENOENT;
}


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p _ b y _ n a m e

    @brief Remove a signal from the specified group by name.

    This function will add a new signal to the specified group.

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group_by_name ( const domain_t domainId,
                                           const char*    name,
                                           const group_t  groupId )
{
    signal_t signalId;

    vsi_name_string_to_id ( domainId, name, &signalId );

    return vsi_remove_signal_from_group ( domainId, signalId, groupId );
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   P r o c e s s i n g
    =========================================

    TODO: Note that the code in all of the functions below that operate on
    everything in a group is similar.  It would be a good idea to abstract
    these operations and use a callback function to specify the specific
    operation to be performed by a generic iterator.


    v s i _ g e t _ n e w e s t _ i n _ g r o u p

    @brief Get the newest signal available for all signals in a group.

    This function will fetch the newest (the one with the most recent
    timestamp) signal available for each member signal in the specified group.

    Note that the result pointer in this case MUST be a pointer to an array of
    result structures sufficiently large to contain one structure for each
    member of the group.  The newest signal information for each member of the
    group will be returned in one of the result structures.  Each result
    structure will also contain the completion code for the signal fetch
    operation that it represents so it is entirely possible for some signals
    to have good completion codes and some to have errors (if a signal does
    not have any data for instance).

    All of the results are retrieved in the "no wait" mode so any signals that
    do not contain any data will have an error code returned for them
    indicating that there was no data available for that signal.

    Also note that none of the signal information will be removed from the
    database during this operation.  Fetches of the "newest" signal data do
    not automatically delete that information from the database.

------------------------------------------------------------------------*/
int vsi_get_newest_in_group ( const group_t groupId,
                              vsi_result*   results )
{
    int                    resultIndex = 0;
    vsi_signal_group*      signalGroup = 0;
    vsi_signal_group_data* groupData = 0;
    signal_list*           signalList = 0;

    printf ( "vsi_get_newest_in_group called with group: %d\n", groupId );

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( groupId && results );

    //
    //  Go get the signal group structure for the specified group id.
    //
    signalGroup = vsi_fetch_signal_group ( groupId );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    offset_t groupDataOffset = signalGroup->head;

    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        //
        //  Get a pointer to the signal group data structure.
        //
        groupData = toAddress ( groupDataOffset );

        //
        //  Get the pointer to the signal list in the group data structure.
        //
        signalList = toAddress ( groupData->signalList );

		//
		//  Populate the current result structure with the signal
		//  identification information.
		//
		results[resultIndex].domainId = signalList->domainId;
		results[resultIndex].signalId = signalList->signalId;

        //
        //  Go get the newest entry in the database for this signal.
        //
        //  Note that the return information will be populated in the result
        //  object that is passed in.
        //
        vsi_get_newest_signal ( &results[resultIndex++] );

        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the newest signal for every member of the specified group.

    TODO: WARNING: Unimplemented!

    The newest member of each signal in the specified group will be retrieved
    and stored in the result array.  If any of the signals in the group do not
    contain any members, execution will be suspended for the current thread
    until some data is received for that member.  When execution resumes, the
    next signal will be fetched and so forth until all of the signals have
    been retrieved.  At that point this function will return to the caller.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures
    must be at least as large as the number of signals defined in the
    specified group.  Failure to allocate enough space for the return data
    will result in memory corruption and undefined behavior (almost always
    very bad!).

    The signal data that is retrieved and returned to the caller will NOT be
    removed from the VSI core database once it is copied into the result
    structures.

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_in_group_wait ( const group_t groupId,
                                   vsi_result*   result )
{
    printf ( "Warning: Unimplemented function \"vsi_get_newest_in_group_wait\" "
             "called - Call ignored!\n" );

    return ENOSYS;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p

    @brief Get the oldest signal available for all signals in a group.

    This function will fetch the oldest (the one with the least recent
    timestamp) signal available for each member signal in the specified group.

    Note that the result pointer in this case MUST be a pointer to an array of
    result structures sufficiently large to contain one structure for each
    member of the group.  The oldest signal information for each member of the
    group will be returned in one of the result structures.  Each result
    structure will also contain the completion code for the signal fetch
    operation that it represents so it is entirely possible for some signals
    to have good completion codes and some to have errors (if a signal does
    not have any data for instance).

    All of the results are retrieved in the "no wait" mode so any signals that
    do not contain any data will have an error code returned for them
    indicating that there was no data available for that signal.

    Also note that the signal information retrieved will be removed from the
    database during this operation.  Fetches of the "oldest" signal data
    automatically delete that information from the database.

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group ( const group_t groupId,
                              vsi_result*   results )
{
    int                    resultIndex = 0;
    vsi_signal_group*      signalGroup = 0;
    vsi_signal_group_data* groupData = 0;
    signal_list*           signalList = 0;
    vsi_result*            result = 0;

    LOG ( "Called vsi_get_oldest_in_group with group: %u, results: %p\n",
          groupId, results );
    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( groupId && results );

    //
    //  Go get the signal group structure for the specified group id.
    //
    signalGroup = vsi_fetch_signal_group ( groupId );

    LOG ( "  Signal group found: %p\n", signalGroup );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }

    // printSignalGroup ( "", signalGroup );

    //
    //  While we have not reached the end of the signal list...
    //
    offset_t groupDataOffset = signalGroup->head;

    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        //
        //  Get a pointer to the signal group data structure.
        //
        groupData = toAddress ( groupDataOffset );

        //
        //  Get the pointer to the signal list in the group data structure.
        //
        signalList = toAddress ( groupData->signalList );

        LOG ( "  Found signal %u, %u\n", signalList->domainId,
              signalList->signalId );
		//
		//  Populate the current result structure with the signal
		//  identification information.
		//
        result = &results[resultIndex++];

		result->domainId    = signalList->domainId;
		result->signalId    = signalList->signalId;
		result->name        = toAddress ( signalList->name );
		result->data        = (char*)&result->literalData;
		result->literalData = 0;
        result->dataLength  = sizeof(unsigned long);

        //
        //  Go get the oldest entry in the database for this signal.
        //
        //  Note that the return information will be populated in the result
        //  object that is passed in.
        //
        vsi_get_oldest_signal ( result );

        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the oldest signal for every member of the specified group.

    TODO: WARNING: Unimplemented!

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures
    must be at least as large as the number of signals defined in the
    specified group.  Failure to allocate enough space for the return data
    will result in memory corruption and undefined behavior (almost always
    very bad!).

    The signal data that is retrieved and returned to the caller will NOT be
    removed from the VSI core database once it is copied into the result
    structures.

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group_wait ( const group_t groupId,
                                   vsi_result*   result )
{
    printf ( "Warning: Unimplemented function \"vsi_get_oldest_in_group_wait\" "
             "called - Call ignored!\n" );

    return ENOSYS;
}


/*!-----------------------------------------------------------------------

    w a i t F o r S i g n a l O n A n y

    @brief Wait for a signal to arrive.

    This is the "main" function for the threads that are created to manage
    the wait for any one of a set of signals.  We will create a thread for
    each of the signals in the set we are waiting for and the first of the
    threads that catches a signal will trigger the end of the wait and all of
    the threads in this set will be destoyed at that point.

    Note that this function does not return a value even though it claims to.
    This is because of the required function signature for callback functions
    to be used with the "pthread_create" call.

    On entry, the results object must contain the data required to make the
    call to the VSI core library to retrieve the specified signal.  That
    includes the domain ID and the signal ID.

    @param[in/out] resultsPtr - The structure to populate with the results data.
    @param[in] threadIds - The array of thread Ids that were created.
    @param[in] numberOfThreads - The number of threads that were created.

    @return None

------------------------------------------------------------------------*/
//
//  Define the data structure that is passed to the threads being created.
//
typedef struct threadData_t
{
    vsi_result* result;
    pthread_t*  threadIds;
    int         numberOfThreads;

}   threadData_t;


static void* waitForSignalOnAny ( void* resultsPtr )
{
    threadData_t* tData  = resultsPtr;
    vsi_result*   result = tData->result;
    int           status = 0;

    LOG ( "New thread %lu has started - Waiting for %d, %d\n",
          pthread_self() % 10000, result->domainId,
          result->signalId );
    //
    //  Go fetch the requested signal from the core data store.  Note that
    //  this function will hang on a semaphore if there is nothing to fetch.
    //  When it returns, it will have gotten the requested signal data.
    //
    status = vsi_core_fetch_wait ( result->domainId,
                                   result->signalId,
                                   (unsigned long*)result->data,
                                   &result->dataLength );
    //
    //  If the above called failed, abort this function and just quit.
    //
    if ( status != 0 )
    {
        return NULL;
    }
    LOG ( "Thread found signal, %d, %d status[%d], with data %u\n",
          result->domainId, result->signalId, status, result->data[0] );
    //
    //  If we get here it's because we were able to fetch the requested signal
    //  data.  At this point, we need to kill all the other threads that have
    //  not received their data yet.  Once we return, the function that
    //  spawned this thread will regain control and handle the returned data.
    //
    LOG ( "Cancelling all the other threads that were waiting...\n" );
    for ( int i = 0; i < tData->numberOfThreads; ++i )
    {
        if ( tData->threadIds[i] != pthread_self() )
        {
            LOG ( "  Cancelling thread %lu\n", tData->threadIds[i] % 10000 );
            pthread_cancel ( tData->threadIds[i] );
        }
    }
    //
    //  Exit the thread.
    //
    return NULL;
}


/*!-----------------------------------------------------------------------

    v s i _ l i s t e n _ a n y _ i n _ g r o u p

    @brief Listen for any signal in the signal list

    This function will listen for any signal in the specified group.  The
    signal list must have been previously built by adding the desired signals
    to the group before making this call.  The first signal received after
    this call is made will return it's value and the function will return to
    the caller.

    Unlike many of the other functions which return data for a group, this
    function does not expect (nor returns) an array of result structures.  It
    only returns a single result for the first signal in the group that
    contains data.

    TODO: Implement the timeout argument.
    TODO: Shouldn't this function really take a "vsi_result" argument?

    @param[out] domainId - The address of where to store the domain ID
    @param[out] signalId - The address of where to store the signal ID
    @param[out] groupId - The address of where to store the group ID
    @param[in] timeout - The optional timeout value in nanoseconds

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
int vsi_listen_any_in_group ( const group_t groupId,
                              unsigned int  timeout,
                              domain_t*     domainId,
                              signal_t*     signalId )
{
    int                    groupCount = 0;
    int                    i = 0;
    int                    status = 0;
    vsi_signal_group       requestedSignalGroup = { 0 };
    vsi_signal_group*      signalGroup = 0;
	vsi_signal_group_data* groupData = 0;
    signal_list*           signalList = 0;
    vsi_result*            result = 0;
    threadData_t*          tData;
    pthread_t*             threadIds = 0;

    //
    //  Make sure the required inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( groupId );

    //
    //  Go get the signal group structure for the specified group id.
    //
    requestedSignalGroup.groupId = groupId;

    signalGroup = btree_search ( &vsiContext->groupIdIndex, &requestedSignalGroup );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }
    //
    //  Allocate the array of thread ids that we will need.
    //
    groupCount = signalGroup->count;
    threadIds = malloc ( groupCount * sizeof(pthread_t) );
    if ( ! threadIds )
    {
        return ENOMEM;
    }
    //
    //  Allocate the array of result structures, one for each thread we are
    //  going to create.
    //
    vsi_result* results = malloc ( groupCount * sizeof(vsi_result) );
    if ( ! results )
    {
        return ENOMEM;
    }
    //
    //  Allocate the array of thread data structures.
    //
    tData = malloc ( groupCount * sizeof(threadData_t) );
    if ( ! tData )
    {
        return ENOMEM;
    }
    //
    //  Get the offset of the first signal in the group's signal list.
    //
    offset_t groupDataOffset = signalGroup->head;

    //
    //  While we have not reached the end of the signal list...
    //
    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        groupData = toAddress ( groupDataOffset );

        //
        //  Get the pointer to the signal list in the group data structure.
        //
        signalList = toAddress ( groupData->signalList );

        LOG ( "  Creating listening thread for %d, %d\n",
              signalList->domainId, signalList->signalId );
        //
        //  Get a pointer to the current result structure.
        //
        result = &results[i];

        //
        //  Initialize the mutex for this result structure.
        //
        pthread_mutex_init ( &result->lock, NULL );

        //
        //  Specify the domain and signal IDs for the current signal in the
        //  results structure that will be passed to the new thread.
        //
        pthread_mutex_lock ( &result->lock );

        result->dataLength = 0;

        result->domainId = signalList->domainId;
        result->signalId = signalList->signalId;

        result->status = ENOENT;

        pthread_mutex_unlock ( &result->lock );

        //
        //  Initialize all of the fields of the thread data structure.
        //
        tData[i].result          = results;
        tData[i].threadIds       = threadIds;
        tData[i].numberOfThreads = groupCount;

        //
        //  Go create a new thread that will wait for the specified signal to
        //  occur in the core data store.
        //
        status = pthread_create ( &threadIds[i], NULL, waitForSignalOnAny, &tData[i] );

        LOG ( "  Created thread %lu\n", threadIds[i] % 10000 );

        ++i;

        //
        //  If we could not create a new thread, return the error code to the
        //  caller.
        //
        if ( status != 0 )
        {
            printf ( "Error creating thread: %m\n" );
            return status;
        }
        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;
    }
    //
    //  Wait for all of the threads to terminate.  Note that under normal
    //  conditions, one of the threads will receive the signal that it has
    //  been waiting for.  When that happens, that thread will then cancel all
    //  of the other threads that are waiting and then terminate itself.  At
    //  that point all of the threads will be dead and this loop should
    //  terminate.
    //
    LOG ( "Waiting for threads to terminate...\n" );

    for ( i = 0; i < groupCount; ++i )
    {
        LOG ( "  Waiting for thread %lu to terminate\n", threadIds[i] % 10000 );
        pthread_join ( threadIds[i], NULL );
        LOG ( "  Thread %lu has terminated\n", threadIds[i] % 10000 );

        //
        //  If this is the thread that received the signal, the status field
        //  in it's result object should be zero.
        //
        if ( results[i].status == 0 )
        {
            //
            //  Return the domain and signal id that we received.
            //
            *domainId = results[i].domainId;
            *signalId = results[i].signalId;
        }
    }
    //
    //  Clean up our temporary memory and return a good completion code to the
    //  caller.
    //
    free ( threadIds );
    free ( results );
    free ( tData );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    w a i t F o r S i g n a l O n A l l

    @brief Wait for all signals to arrive for a group.

    This is the "main" function for the threads that are created to manage the
    wait for all of the signals in a specified set.  We will create a thread
    to fetch each individual signal in the desired set and as each thread
    receives it's signal, the results for that signal will be recorded in the
    results structure assigned to it and then terminate.  When all of the
    threads have terminated, the calling function will resume and the results
    array returned to the caller.

    The caller must supply a pointer to a "vsi_result" structure that can be
    used to record the result data.  Each thread will record it's fetch signal
    data in one of these structures.

    Note that this function does not return a value even though it claims to.
    This is because of the required function signature for callback functions
    to be used with the "pthread_create" call.

    On entry, the results object must contain the data required to make the
    call to the VSI core library to retrieve the specified signal.  That
    includes the domain ID and the signal ID.

    @param[in/out] resultsPtr - The structure to populate with the results data.

    @return None

------------------------------------------------------------------------*/
static void* waitForSignalOnAll ( void* resultsPtr )
{
    threadData_t* tData  = resultsPtr;
    vsi_result*   result = tData->result;
    int           status = 0;

    LOG ( "New thread %lu has started - Waiting for %d, %d\n",
          pthread_self() % 10000, result->domainId, result->signalId );
    //
    //  Go fetch the requested signal from the core data store.  Note that
    //  this function will hang on a semaphore if there is nothing to fetch.
    //  When it returns, it will have gotten the requested signal data.
    //
    status = vsi_core_fetch_wait ( result->domainId,
                                   result->signalId,
                                   (unsigned long*)result->data,
                                   &result->dataLength );
    //
    //  If the above called failed, abort this function and just quit.
    //
    if ( status != 0 )
    {
        return NULL;
    }
    LOG ( "Thread found signal, %d, %d status[%d], with data %u\n",
          result->domainId, result->signalId, status, result->data[0] );
    //
    //  Exit the thread.
    //
    return NULL;
}


/*!-----------------------------------------------------------------------

    v s i _ l i s t e n _ a l l _ i n _ g r o u p

    @brief Listen for all signals in the specified group

    This function will listen for all signals in the specified group.  The
    signal list must have been previously built by adding the desired signals
    to the group before making this call.  When all of the signals defined in
    the specified group have received a signal, this call will return to the
    caller.  This function will hang until all of the specified signals have
    been received.

    Note that the results pointer argument here MUST be a pointer to an array
    of vsi_result structures and that array must be at least as large as the
    number of signals that are expected.  The resultsSize argument is the
    number of structures being supplied (NOT the total size of the results
    array in bytes!).  If the resultsSize is not large enough to hold all of
    the results required, an error will be returned to the caller.

    TODO: Implement the timeout argument.

    @param[in] groupId - The ID of the group to listen for
    @param[out] resultsPtr - The address of where to store the results
    @param[in] resultsSize - The size of the results array being supplied
    @param[in] timeout - The optional timeout value in nanoseconds

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            EINVAL - There are no signals or groups to listen to.
            ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
int vsi_listen_all_in_group ( const group_t groupId,
                              vsi_result*   results,
                              unsigned int  resultsSize,
                              unsigned int  timeout )
{
    int                    groupCount = 0;
    int                    i = 0;
    int                    status = 0;
    vsi_signal_group       requestedSignalGroup = { 0 };
    vsi_signal_group*      signalGroup = 0;
    vsi_signal_group_data* groupData = 0;
    signal_list*           signalList = 0;
    vsi_result*            result = 0;
    threadData_t*          tData;
    pthread_t*             threadIds = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( groupId && results && resultsSize );

    //
    //  Go get the signal group structure for the specified group id.
    //
    requestedSignalGroup.groupId = groupId;

    signalGroup = btree_search ( &vsiContext->groupIdIndex, &requestedSignalGroup );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }
    //
    //  If the supplied results structure array is not large enough to hold
    //  all of the results we will get, abort this call with an error code.
    //
    groupCount = signalGroup->count;
    if ( resultsSize < groupCount )
    {
        return ENOMEM;
    }
    //
    //  Allocate the array of thread ids that we will need.
    //
    threadIds = malloc ( groupCount * sizeof(pthread_t) );
    if ( ! threadIds )
    {
        return ENOMEM;
    }
    //
    //  Allocate the array of thread data structures.
    //
    tData = malloc ( groupCount * sizeof(threadData_t) );
    if ( ! tData )
    {
        return ENOMEM;
    }
    //
    //  Get the offset of the first signal in the group's signal list.
    //
    offset_t groupDataOffset = signalGroup->head;

    //
    //  While we have not reached the end of the signal list...
    //
    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        groupData = toAddress ( groupDataOffset );

        //
        //  Get the pointer to the signal list in the group data structure.
        //
        signalList = toAddress ( groupData->signalList );

        LOG ( "  Creating listening thread for %d, %d\n",
              signalList->domainId, signalList->signalId );
        //
        //  Get a pointer to the current result structure.
        //
        result = &results[i];

        //
        //  Initialize the mutex for this result structure.
        //
        pthread_mutex_init ( &result->lock, NULL );

        //
        //  Specify the domain and signal IDs for the current signal in the
        //  results structure that will be passed to the new thread.
        //
        pthread_mutex_lock ( &result->lock );

        result->dataLength = 0;

        result->domainId = signalList->domainId;
        result->signalId = signalList->signalId;

        result->status = ENOENT;

        pthread_mutex_unlock ( &result->lock );

        //
        //  Initialize all of the fields of the thread data structure.
        //
        tData[i].result          = results;
        tData[i].threadIds       = NULL;
        tData[i].numberOfThreads = 0;

        //
        //  Go create a new thread that will wait for the specified signal to
        //  occur in the core data store.
        //
        status = pthread_create ( &threadIds[i], NULL, waitForSignalOnAll, &tData[i] );

        LOG ( "  Created thread %lu\n", threadIds[i] % 10000 );

        ++i;

        //
        //  If we could not create a new thread, return the error code to the
        //  caller.
        //
        if ( status != 0 )
        {
            printf ( "Error creating thread: %m\n" );
            return status;
        }
        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;
    }
    //
    //  Wait for all of the threads to terminate.  Note that under normal
    //  conditions, one of the threads will receive the signal that it has
    //  been waiting for.  When that happens, that thread will then cancel all
    //  of the other threads that are waiting and then terminate itself.  At
    //  that point all of the threads will be dead and this loop should
    //  terminate.
    //
    LOG ( "Waiting for threads to terminate...\n" );

    for ( i = 0; i < groupCount; ++i )
    {
        LOG ( "  Waiting for thread %lu to terminate\n", threadIds[i] % 10000 );
        pthread_join ( threadIds[i], NULL );
        LOG ( "  Thread %lu has terminated\n", threadIds[i] % 10000 );
    }
    //
    //  Clean up our temporary memory and return a good completion code to the
    //  caller.
    //
    free ( threadIds );
    free ( tData );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ g r o u p

    @brief Flush all of the signals available for all signals in a group.

    This function will flush all of the signals available for each member
    signal in the specified group.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the queues of every
    signal in the group.

------------------------------------------------------------------------*/
int vsi_flush_group ( const group_t groupId )
{
    vsi_signal_group       requestedSignalGroup = { 0 };
    vsi_signal_group*      signalGroup = 0;
    vsi_signal_group_data* groupData = 0;
    signal_list*           signalList = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( groupId );

    //
    //  Go get the signal group structure for the specified group id.
    //
    requestedSignalGroup.groupId = groupId;

    signalGroup = btree_search ( &vsiContext->groupIdIndex, &requestedSignalGroup );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( signalGroup == NULL )
    {
        return ENOENT;
    }
	//
    //  Get the offset of the first signal in the group's signal list.
    //
    offset_t groupDataOffset = signalGroup->head;

    //
    //  If the head pointer is NULL, the list is empty so return an error code
    //  to the caller.
    //
    if ( groupDataOffset == END_OF_LIST_MARKER )
    {
        return ENOENT;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
        groupData = toAddress ( groupDataOffset );

        //
        //  Get the pointer to the signal list in the group data structure.
        //
        signalList = toAddress ( groupData->signalList );

        //
        //  Go flush all of the signal data for this signal.
        //
        vsi_flush_signal ( signalList->domainId, signalList->signalId );

        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    N a m e / I D   M a n i u p l a t i o n   F u n c t i o n s
    ===========================================================


    v s i _ n a m e _ s t r i n g _ t o _ i d

    @brief Convert a domain and signal name to it's signal ID.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain.

    The domainId is the ID of the domain in which the name is defined.

    The signalId is a pointer to an identifier in which the signal ID will
    be stored.

    @param[in] - name - The name of the signal to be converted.
    @param[in] - domainId - The domain of the name to look up.
    @param[out] - signalId - A pointer in which the signal ID will be stored.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_name_string_to_id ( domain_t    domainId,
                            const char* name,
                            signal_t*   signalId )
{
    signal_list* signalList = 0;

    LOG ( "vsi_name_string_to_id called with domain %d, name[%s]\n", domainId, name );

    //
    //  Make sure all of the required input arguments are supplied.
    //
    CHECK_AND_RETURN_IF_ERROR ( name && domainId && signalId );

	//
	//	Allocate a scratch signal list to use for the name lookup in the btrees.
	//
    signal_list* requestedSignalList = sm_malloc ( sizeof(signal_list) );

    //
    //  Allocate a shared memory buffer for the target name string and a
    //  pointer to that buffer.
    //
    int nameLen = strlen(name) + 1;

    char* tempName = sm_malloc ( nameLen );

    requestedSignalList->name = toOffset ( tempName );
    requestedSignalList->domainId = domainId;

    //
    //  Copy the target signal name into the shared memory buffer area.
    //
    strncpy ( tempName, name, nameLen - 1 );
    tempName[nameLen-1] = 0;

    //
    //  Go find the record that has this name.
    //
    signalList = btree_search ( &vsiContext->signalNameIndex, requestedSignalList );

    //
    //  Deallocate the temporary target name string buffer and request structure.
    //
    sm_free ( tempName );
    sm_free ( requestedSignalList );

    //
    //  If the search failed, return an error code to the caller.  The name
    //  that was specified does not exist in our indices.
    //
    if ( signalList == NULL )
    {
        return EINVAL;
    }
    //
    //  Copy the signal id from the returned data structure into the caller's
    //  return value pointer.
    //
    *signalId = signalList->signalId;

    LOG ( "vsi_name_string_to_id returning domain: %d, signal: %d\n",
          signalList->domainId, signalList->signalId );
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}

/*!-----------------------------------------------------------------------

    v s i _ s i g n a l _ i d _ t o _ s t r i n g

    @brief Convert a signal domain and ID to it's ASCII string name.

    The domainId is the domain ID to be converted.

    The signalId is the signal ID to be converted.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The name pointer must be a preallocated memory area that is large enough
    to contain the name of the signal being asked for.  The name of the signal
    will be copied into that buffer and null terminated.

    The nameLength is the maximum number of bytes of data that will be copied
    into the supplied buffer by the API code.  This value should be set to the
    maximum size of the name buffer being supplied.  If the name buffer
    supplied is not large enough to contain the entire name, the data will be
    truncated to fit into the buffer.

    @param[in] - domainId - The signal domain ID to be converted.
    @param[in] - signalId - The signal ID to be converted.
    @param[in/out] - name - A pointer to a buffer in which to store the name.
    @param[in] - nameLength - The size of the name buffer being supplied.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_signal_id_to_string ( const domain_t domainId,
                              const signal_t signalId,
                              char**         name,
                              int            nameLength )
{
    signal_list* signalList = 0;

    //
    //  Make sure all of the required input arguments are supplied.
    //
    CHECK_AND_RETURN_IF_ERROR ( name );

    //
    //  Allocate a scratch signal list to use for the lookup in the btrees.
    //
    signal_list* requestedSignalList = sm_malloc ( sizeof(signal_list) );

    //
    //  Initialize the id fields for the lookup operation.
    //
    requestedSignalList->domainId = domainId;
    requestedSignalList->signalId = signalId;

    //
    //  Go find the record that has this name.
    //
    signalList = btree_search ( &vsiContext->signalIdIndex, requestedSignalList );

    //
    //  Deallocate the request structure.
    //
    sm_free ( requestedSignalList );

    //
    //  If the search failed, return an error code to the caller.  The ids
    //  that were specified do not exist in our indices.
    //
    if ( signalList == NULL )
    {
        return EINVAL;
    }
    //
    //  Copy the domain and signal ids from the returned data structure into
    //  the caller's return value pointers.
    //
    strncpy ( *name, toAddress ( signalList->name ), nameLength );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ d e f i n e _ s i g n a l

    @brief Define a new signal list definition record.

    This function will define a new signal list definition record.  This
    record will be inserted into the signal Id and signal Name btree indices
    to allow conversions between the Id and Name of a signal and vice versa.
    Afer this function has been called, the structure that was created can be
    found in the btrees using the normal btree lookup functions.

    The privateId will only be inserted into it's btree if the input value is
    not zero.

    The name string will only be inserted into it's btree if the input value
    is not null.

    Use this function to create an empty signal list structure in the btrees.
    This is useful when reading in a vechicle signal specification file for
    instance.

    @param[in] domainId - The signal domain ID to be defined.
    @param[in] signalId - The signal ID to be defined.
    @param[in] privateId - The private ID to be defined.
    @param[in] name - The ASCII name of the signal to be defined.

    @return status - The return status of the function (0 = Good)

------------------------------------------------------------------------*/
int vsi_define_signal ( const domain_t domainId,
                        const signal_t signalId,
                        const signal_t privateId,
                        const char*    name )
{
    LOG ( "Defining: domainId: %d, signalId: %d, privateId: %d, name[%s]\n",
          domainId, signalId, privateId, name );
    //
    //  Go allocate a new signal list data structure in the shared memory
    //  area and initialize it to all zeroes.
    //
    signal_list* signalList = sm_malloc ( sizeof(signal_list) );
    memset ( signalList, 0, sizeof(signal_list) );

    //
    //  Initialize the ID fields of the structure.
    //
    signalList->domainId  = domainId;
    signalList->signalId  = signalId;
    signalList->privateId = privateId;

    //
    //  Go insert the new signal list structure into the ID btree.
    //
    btree_insert ( &vsiContext->signalIdIndex, signalList );

    //
    //  If there is a private ID for this signal, add it's definition to the
    //  private index as well.
    //
    if ( privateId != 0 )
    {
        btree_insert ( &vsiContext->privateIdIndex, signalList );
    }
    //
    //  If a name was specified, allocate a buffer for the name string and
    //  store it in the new structure.
    //
    if ( name != NULL )
    {
        //
        //  Get the length of the name (with the terminating null byte).
        //
        int nameLen = strlen(name) + 1;

        //
        //  Allocate a shared memory buffer to hold the name.
        //
        char* tempName = sm_malloc ( nameLen );
        memset ( tempName, 0, nameLen );

        //
        //  Store the offset to the new name buffer in the signal list
        //  structure.
        //
        signalList->name = toOffset ( tempName );

        //
        //  Copy the input name string into the new buffer.
        //
        strncpy ( tempName, name, nameLen - 1 );

        //
        //  Go add this signal list structure to the signal-name btree index.
        //
        btree_insert ( &vsiContext->signalNameIndex, signalList );
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!----------------------------------------------------------------------------

    S i g n a l   D u m p   F u n c t i o n s

-----------------------------------------------------------------------------*/

//
//  Declare the signal dump functions.
//
void dumpSignals ( void );

void signalTraverseFunction ( char* leader, void* dataPtr );

void printSignalList ( signal_list* signalList, int maxSignals );

void printSignalData ( signal_list* signalList, int maxSignals );

//
//  Declare the signal group dump function.
//
void dumpGroups ( void );

void groupTraverseFunction ( char* leader, void* dataPtr );

void printSignalGroup ( char* leader, void* data );


/*!----------------------------------------------------------------------------

    d u m p S i g n a l s

    @brief Dump all of the currently defined signals by ID.

-----------------------------------------------------------------------------*/
void dumpSignals ( void )
{
    //
    //  Go print all of the currently defined signal list structures.
    //
	printf ( "\nThe defined signals in ID order:...\n\n" );
	btree_traverse ( &vsiContext->signalIdIndex, signalTraverseFunction );

	printf ( "\nThe defined signals in name order:...\n\n" );
	btree_traverse ( &vsiContext->signalNameIndex, signalTraverseFunction );

	printf ( "\nThe defined signals in private ID order:...\n\n" );
	btree_traverse ( &vsiContext->privateIdIndex, signalTraverseFunction );

    return;
}


/*!----------------------------------------------------------------------------

    s i g n a l T r a v e r s e F u n c t i o n

    @brief Callback function for the btree traversal function that will print
    each signal defined in the system.

-----------------------------------------------------------------------------*/
void signalTraverseFunction ( char* leader, void* dataPtr )
{
    if ( dataPtr == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    printSignalList ( dataPtr, 5 );
}


/*!-----------------------------------------------------------------------

    p r i n t S i g n a l L i s t

    @brief Print the contents of the specified signal list.

    This function will print the contents of a single signal list.  This will
    also print the individual signals contained in this list.

    @param[in] signalList - The address of the signal list to be printed
    @param[in] maxSignals - The maximum number of signals to display in each
                            signal list.
    @return None

------------------------------------------------------------------------*/
void printSignalList ( signal_list* signalList, int maxSignals )
{
    //
    //  If the system has not finished initializing yet, just ignore this call.
    //
    if ( ! smControl->systemInitialized )
    {
        return;
    }
    //
    //  Display the address of this signal list and it's offset within the
    //  shared memory segment.
    //
    printf ( "Signal List %p[0x%lx]:\n", signalList, toOffset ( signalList ) );

    //
    //  Display the signal list keys.
    //
    printf ( "  Domain ID........: %d\n", signalList->domainId );
    printf ( "  Signal ID........: %d\n", signalList->signalId );
    printf ( "  Private ID.......: %d\n", signalList->privateId );

    //
    //  Diisplay the signal name if it's defined.
    //
    if ( signalList->name != 0 )
    {
        printf ( "  Signal Name......: [%s]\n",
              (char*)toAddress ( signalList->name ) );
    }
    else
    {
        printf ( "  Signal Name......: [null]\n" );
    }
    //
    //  If there are some signals in this signal list...
    //
    if ( signalList->currentSignalCount > 0 )
    {
        //
        //  If the head offset indicates that this is the end of the signal
        //  list, display a signal saying we hit the end of the list.
        //
        if ( signalList->head == END_OF_LIST_MARKER )
        {
            printf ( "  Head offset......: End of List Marker\n" );
        }
        //
        //  Otherwise, display the head offset value.  This is the offset in
        //  the segment of the first data signal record.
        //
        else
        {
            printf ( "  Head offset......: 0x%lx[%lu]\n", signalList->head,
                  signalList->head );
        }
        //
        //  If the tail offset indicates that this is the end of the signal
        //  list, display a signal saying we hit the end of the list.
        //
        if ( signalList->tail == END_OF_LIST_MARKER )
        {
            printf ( "  Tail offset......: End of List Marker\n" );
        }
        //
        //  Otherwise, display the tail offset value.  This is the offset in
        //  the segment of the start of the last data signal record.
        //
        else
        {
            printf ( "  Tail offset......: 0x%lx[%lu]\n", signalList->tail,
                  signalList->tail );
        }
        //
        //  Display the number of messages currently stored in this hash
        //  bucket.
        //
        printf ( "  Signal count.....: %'lu\n", signalList->currentSignalCount );

        //
        //  Display the number of bytes of memory that have been allocated to
        //  this signal list for the storage of signals.
        //
        printf ( "  Total signal size: %lu[0x%lx]\n", signalList->totalSignalSize,
              signalList->totalSignalSize );

        //
        //  Go print the contents of the signal list data in this entry.
        //
        printSignalData ( signalList, maxSignals );
    }
    //
    //  If the signal list is empty, just tell the user that.
    //
    else
    {
        printf ( "  [empty signal list]\n" );
    }
    return;
}


/*!-----------------------------------------------------------------------

    p r i n t S i g n a l D a t a

    @brief Print the contents of the specified shared memory signal list data.

    This function will print all of the signal data records in the specified
    signal list.  The actual binary contents of each message is also printed.

    @param[in] signalList - The address of the signalList to be dumped.
    @param[in] maxSignals - The maximum number of signals to dump for this
                            signal list.
    @return None

------------------------------------------------------------------------*/
void printSignalData ( signal_list* signalList, int maxSignals )
{
    int i = 0;

    //
    //  If the system has not finished initializing yet, just ignore this call.
    //
    if ( ! smControl->systemInitialized )
    {
        return;
    }
    //
    //  If the user did not specify a maximum signal count, set it to a large
    //  number.
    //
    if ( maxSignals == 0 )
    {
        maxSignals = INT_MAX;
    }
    //
    //  Get the offset to the first signal in this list.
    //
    offset_t signalOffset = signalList->head;

    //
    //  If this signal list is empty, just silently quit.
    //
    if ( signalList->currentSignalCount <= 0 )
    {
        return;
    }
    //
    //  Get an actual pointer to the first signal in this list.  This is
    //  the signal that is pointed to by the "head" offset.
    //
    signal_data* signal = toAddress ( signalOffset );

    //
    //  Display the signal list address.
    //
    printf ( "  Signal list [%p:0x%lx]:\n", signalList, signalOffset );

    //
    //  Repeat until we reach the end of the signal list in this bucket.
    //
    while ( signalOffset != END_OF_LIST_MARKER )
    {
        //
        //  Display the size of this signal in bytes.
        //
        printf ( "    %'d - 0x%lx Signal data size...: %'lu\n", ++i,
                 signalOffset, signal->messageSize );
        //
        //  Go dump the contents of the data field for this signal.
        //
        HexDump ( signal->data, signal->messageSize, "Signal Data", 10 );

        //
        //  If the "next" signal offset indicates that this is the end of the
        //  signal list, display a message saying we hit the end of the list.
        //
        signalOffset = signal->nextMessageOffset;
        if ( signalOffset == END_OF_LIST_MARKER )
        {
            printf ( "        Next signal offset.: End of List Marker\n" );
        }
        //
        //  Otherwise, display the next signal offset value.  This is the
        //  offset in the signal list where the next logical signal in the
        //  list is located.
        //
        else
        {
            printf ( "        Next signal offset.: 0x%lx[%'lu]\n",
                     signalOffset, signalOffset );
        }
        //
        //  If we have reached the maximum number of signals the caller asked
        //  us to disply, break out of this loop and quit.
        //
        if ( --maxSignals == 0 )
        {
            break;
        }
        //
        //  Get the address of where the next signal in the list is located.
        //
        signal = toAddress ( signalOffset );
    }
    return;
}


/*!----------------------------------------------------------------------------

    d u m p G r o u p s

    @brief Dump all of the signal groups defined in the shared memory segment.

-----------------------------------------------------------------------------*/
void dumpGroups ( void )
{
    printf ( "The defined groups in ID order:...\n\n" );

    //
    //  Go print all of the currently defined group records.
    //
    btree_traverse ( &vsiContext->groupIdIndex, groupTraverseFunction );

    return;
}


/*!----------------------------------------------------------------------------

    g r o u p T r a v e r s e F u n c t i o n

    @brief

-----------------------------------------------------------------------------*/
void groupTraverseFunction ( char* leader, void* dataPtr )
{
    if ( dataPtr == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    printSignalGroup ( leader, dataPtr );
}


/*!----------------------------------------------------------------------------

    p r i n t S i g n a l G r o u p

    @brief

    This function will be called by the btree code when it has been called to
    print out the btree nodes in the groupIdIndex B-tree.  Each data record
    that is encountered in that process will result in a call to this function
    with the address of the record just found, to render the data the way the
    user wants it to be displayed.

    Note: The data input to this function is actually "vsi_signal_group*".

-----------------------------------------------------------------------------*/
void printSignalGroup ( char* leader, void* data )
{
    vsi_signal_group_data* groupData;
    signal_list*           signalList;

    if ( data == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    vsi_signal_group* signalGroup = data;

    //
    //  Print the list header record.
    //
    printf ( "%sSignal Group:    %d\n", leader, signalGroup->groupId );

    //
    //  Print the number of signals defined in this group.
    //
    printf ( "%s   Signal Count: %d\n", leader, signalGroup->count );

    //
    //  Get the offset of the beginning of the signal list.
    //
    offset_t groupDataOffset = signalGroup->head;

    //
    //  Print the offset of the beginning of the signal list.
    //
    if ( groupDataOffset == END_OF_LIST_MARKER )
	{
        printf ( "%s   Head Offset.: End of List Marker\n", leader );
	}
	else
	{
        printf ( "%s   Head Offset.: 0x%lx[%lu]\n", leader, groupDataOffset,
                 groupDataOffset );
	}
    //
    //  Print the offset of the end of the signal list.
    //
    if ( signalGroup->tail == END_OF_LIST_MARKER )
	{
        printf ( "%s   Tail Offset.: End of List Marker\n", leader );
	}
	else
	{
        printf ( "%s   Tail Offset.: 0x%lx[%lu]\n", leader, signalGroup->tail,
                 signalGroup->tail );
	}
    //
    //  If the list is empty, tell the caller that and quit.
    //
    if ( groupDataOffset == END_OF_LIST_MARKER )
    {
        printf ( "%s   Group has no signals defined.\n", leader );
        return;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( groupDataOffset != END_OF_LIST_MARKER )
    {
		//
        //  Get an actual pointer to this signal in this list.
		//
        groupData = toAddress ( groupDataOffset );

        //
        //  Now get the address of the signal list structure.
        //
        signalList = toAddress ( groupData->signalList );

        //
        //  Go print the contents of the current signal structure.
        //
        printf ( "%s      Domain: %d, Signal: %d, Name:[%s]\n", leader,
                 signalList->domainId, signalList->signalId,
                 (char*)toAddress ( signalList->name ) );
        //
        //  Get the next signal definition object in the list for this group.
        //
        groupDataOffset = groupData->nextMessageOffset;
    }
    return;
}


/******************************************************************************

        H e x D u m p

    This function produces a hex/ASCII dump of the input data similar to:

    Optional title string (25 bytes @ 0x86e9820):
    0000   00 00 00 00 0B 00 00 00 FF FF FF FF 64 65 62 75  ............debu
    0016   67 4C 65 76 65 6C 3A 00 98                       gLevel:..

    Note that the input does not need to be a string, just an array of bytes.
    The dump will be output with 16 bytes per line and have the ASCII
    representation of the data appended to each line.  Unprintable characters
    are printed as a period.

    The title, outputWidth, and leadingSpaces are optional arguments that can
    be left off by using one of the helper macros.

    The output from calling this function will be generated on the "stdout"
    device.

    The maximum length of a hex dump is specified by the constant below and
    all dumps will be truncated to this length if they are longer than this
    maximum value.

    This function should be used as follows:

        HexDump ( data, length, "Title", 0 );

    or in this case:

        HEX_DUMP_TL ( data, length, "Title", 0 );

    Input Parameters:

        data - The address of the data buffer to be dumped
        length - The number of bytes of data to be dumped
        title - The address of an optional title string for the dump
        leadingSpaces - The number of spaces to be added at the beginning of
                        each line.

    Output Data:  None

    Return Value: None

******************************************************************************/

//
//    The following macro is designed for debugging this function.  To enable
//    it, define this as a synonym for "printf".
//
// #define XPRINT printf
#define XPRINT(...)

#define MAX_DUMP_SIZE ( 1024 )

//
//  The following macros are defined in the header file to make it easier to
//  call the HexDump function with various combinations of arguments.
//
// #define HEX_DUMP(   data, length )         HexDump ( data, length, "", 0 )
// #define HEX_DUMP_T( data, length, title )  HexDump ( data, length, title, 0 )
// #define HEX_DUMP_L( data, length, spaces ) HexDump ( data, length, "", spaces )
//

void HexDump ( const char *data, int length, const char *title,
               int leadingSpaces )
{
    unsigned char c;
    int           i;
    int           j;
    int           s                = 0;
    int           outputWidth      = 16;
    int           currentLineWidth = 0;
    int           originalLength   = 0;
    char          asciiString[400] = { 0 };

    XPRINT ( "data(%d): \"%s\", title: %p, width: %d, spaces: %d\n", length,
             data, title, outputWidth, leadingSpaces );
    //
    //    If the length field is unreasonably long, truncate it.
    //
    originalLength = length;
    if ( length > MAX_DUMP_SIZE )
    {
        //
        //    Set the length to the maximum value we will allow.
        //
        length = MAX_DUMP_SIZE;
    }
    //
    //    Output the leader string at the beginning of the title line.
    //
    for ( i = 0; i < leadingSpaces; i++ )
    {
        putchar ( ' ' );
    }
    //
    //    If the user specified a title for the dump, output that title
    //    followed by the buffer size and location.
    //
    if ( title != NULL && strlen(title) != 0 )
    {
        fputs ( title, stdout );
        printf ( " (%d bytes @ %p):\n", originalLength, data );
    }
    //
    //    If the user did not specify a title for the dump, just output the
    //    buffer size and location.
    //
    else
    {
        printf ( "%d bytes @ %p:\n", originalLength, data );
    }
    //
    //    For each byte in the data to be dumped...
    //
    for ( i = 0; i < length; i++ )
    {
        //
        //    If this is the beginning of a new line...
        //
        if ( i % outputWidth == 0 )
        {
            XPRINT ( "New line offset %d", i );

            //
            //    Reset the number of fields output on this line so far.
            //
            currentLineWidth = 0;

            //
            //    If this is not the beginning of the first line, append the
            //    ascii interpretation to the hex dump data at the end of the
            //    line.
            //
            if ( i > 0 )
            {
                asciiString[s] = 0;
                puts ( asciiString );

                asciiString[0] = 0;
                s = 0;
            }
            //
            //    Output the leader string at the beginning of the new line.
            //
            for ( j = 0; j < leadingSpaces; j++ )
            {
                putchar ( ' ' );
            }
            //
            //    Output the data offset field for this new line.
            //
            printf ( "%06d  ", i );
        }
        //
        //    Generate the hex representation for the current data character.
        //
        c = *data++;
        printf ( "%02x ", c );

        //
        //    Generate the ASCII representation of the data character at this
        //    position if it is a printable character.  If it is not, generate
        //    a period in it's place.
        //
        if ( c >= ' ' && c <= '~' )
        {
            asciiString[s++] = c;
        }
        else
        {
            asciiString[s++] = '.';
        }
        //
        //    Increment the width of the line generated so far.
        //
        currentLineWidth++;
    }
    //
    //    We have reached the end of the data to be dumped.
    //
    //    Pad the rest of the fields on this line with blanks so the ASCII
    //    representation will line up with previous lines if it is an
    //    incomplete line.
    //
    for ( i = 0; i < outputWidth - currentLineWidth; i++ )
    {
        fputs ( "   ", stdout );
    }
    //
    //    Output the last line of the display.
    //
    asciiString[s] = 0;
    puts ( asciiString );

    //
    //    If the original length was truncated because it was unreasonably
    //    long then output a message indicating that the dump was truncated.
    //
    if ( originalLength > length )
    {
        printf ( "       ...Dump of %d bytes has been truncated\n", originalLength );
    }
    //
    //    Return to the caller.
    //
    return;
}

