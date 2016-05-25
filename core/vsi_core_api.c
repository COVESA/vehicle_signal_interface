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
#include "sharedMemory.h"


/*! @{ */


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
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

    @return None

------------------------------------------------------------------------*/
void vsi_core_insert ( vsi_core_handle handle, enum domains domain,
                       offset_t key, unsigned long newMessageSize,
                       void* body )
{
    //
    //  Go insert this key into the core data store.
    //
    sm_insert ( handle, domain, key, newMessageSize, body );

    //
    //  Return to the caller.
    //
    return;
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
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the message data.

    @return 0 if successful.
    @return not 0 on error which will be an errno value.

------------------------------------------------------------------------*/
int vsi_core_fetch_wait ( vsi_core_handle handle, enum domains domain,
                          offset_t key, unsigned long* bodySize,
                          void* body )
{
    return sm_fetch ( handle, domain, key, bodySize, body, false );
}


int vsi_core_fetch ( vsi_core_handle handle, enum domains domain,
                     offset_t key, unsigned long* bodySize,
                     void* body )
{
    return sm_fetch ( handle, domain, key, bodySize, body, true );
}


/*! @} */

// vim:filetype=c:syntax=c
