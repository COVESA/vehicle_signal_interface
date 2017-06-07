/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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

#include "sharedMemory.h"
#include "vsi_core_api.h"
#include "signals.h"


/*! @{ */


//
//  Declare the local static functions.
//
static sharedMemory_t* vsi_core_open_user ( bool createNew );
static sysMemory_t*    vsi_core_open_sys  ( bool createNew );

static void vsi_core_close_user ( void );
static void vsi_core_close_sys  ( void );


/*!----------------------------------------------------------------------------

    v s i _ c o r e _ o p e n

    @brief Open the shared memory segments.

    This function will

    @param[in]
    @param[out]
    @param[in,out]

    @return None

-----------------------------------------------------------------------------*/
void vsi_core_open ( bool createNew )
{
    sysControl = vsi_core_open_sys ( createNew );
    if ( sysControl == 0 )
    {
        printf ( "Error: Unable to create the system shared memory segment - "
                 "aborting\n" );
        exit ( 255 );
    }

    smControl = vsi_core_open_user ( createNew );
    if ( smControl == 0 )
    {
        printf ( "Error: Unable to create the user shared memory segment - "
                 "aborting\n" );
        exit ( 255 );
    }

    LOG ( "VSI core data store has been mapped into memory\n" );

    return;
}


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ o p e n _ u s e r

    TODO: Verify text...

    @brief Open the shared memory segment.

    This function will attempt to open the configured shared memory segment.
    If the shared memory segment pseudo-file does not exist or is empty, a new
    shared memory segment will be initialized with all of the appropriate
    parameters and values set.

    If the "createNew" argument is supplied as "true" then the shared memory
    segment file will be deleted before being recreated, thereby creating a
    brand new empty data store.  If this flag is not "true" then the existing
    (if there is one) data store will simply be mapped in and opened.

    If another thread or process has the same data store open when this
    function is called with "createNew", the current connections will not be
    affected and a new data store instance will be created for this call.
    Once the last thread/process that has the old instance open closes that
    instance, the old instance will be completely removed from the file system
    and no longer accessible.

    Note that the base address of the shared memory segment will vary and
    cannot be guaranteed to be the same from one invocation to another.  All
    references to data in the shared memory segment MUST be made with offsets
    relative to the beginning of the shared memory segment and when a real
    pointer is needed (for accessing a particular data buffer for instance),
    the address of the desired data must be dynamically calculated by adding
    the base address of the shared memory segment to any offset that is
    desired.

    @param[in] createNew - The "create a new DB flag"

    @return A handle to the shared memory segement that was opened.

            On error, a null pointer is returned and errno will be set to
            indicate the error that was encountered.

------------------------------------------------------------------------*/
static sharedMemory_t* vsi_core_open_user ( bool createNew )
{
    //
    //  If the caller wants us to create a brand new empty data store...
    //
    if ( createNew )
    {
        unlink ( SHARED_MEMORY_SEGMENT_NAME );
    }
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
    int         status;
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
    //  If this is an empty file...
    //
    LOG ( "  User shared memory segment is %'ld bytes in size\n", stats.st_size );

    if ( stats.st_size <= 0 )
    {
        LOG ( "VSI core data store[%s] is uninitialized - Initializing it...\n",
              SHARED_MEMORY_SEGMENT_NAME );
        //
        //  Go initialize the shared memory segment.
        //
        smControl = sm_initialize ( fd, INITIAL_SHARED_MEMORY_SIZE );
        if ( smControl == 0 )
        {
            printf ( "Unable to initialize the VSI core data store[%s] errno: "
                     "%u[%m].\n", SHARED_MEMORY_SEGMENT_NAME, errno );
            (void) close ( fd );
            return 0;
        }
		//
		//  Allocate our system context data structure.
		//
		smControl->vsiContextOffset = toOffset ( sm_malloc ( sizeof(vsi_context) ) );
		if ( !smControl->vsiContextOffset )
		{
			printf ( "Error: Unable to allocate memory for the VSI context "
					 "- Aborting!\n" );
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
        smControl = mmap ( NULL, stats.st_size, PROT_READ|PROT_WRITE,
                           MAP_SHARED, fd, 0 );
        if ( smControl == MAP_FAILED )
        {
            printf ( "Unable to map the VSI core data store. errno: %u[%m].\n",
                     errno );
            return 0;
        }
    }
    //
    //  Set the global VSI context pointer to the current structure address.
    //
    vsiContext = toAddress ( smControl->vsiContextOffset );

    LOG ( "VSI user data store at %p for %'lu bytes has been mapped into "
          "memory\n", smControl, smControl->sharedMemorySegmentSize );
    //
    //  Close the shared memory pseudo file as we don't need it any more.
    //
    (void) close ( fd );

    //
    //  Return the address of the memory mapped shared memory segment to the
    //  caller.
    //
    return smControl;
}



/*!----------------------------------------------------------------------------

    TODO: Finish header!

    v s i _ c o r e _ o p e n _ s y s

    @brief

    This function will

    @param[in]
    @param[out]
    @param[in,out]

    @return None

-----------------------------------------------------------------------------*/
static sysMemory_t* vsi_core_open_sys ( bool createNew )
{
    //
    //  If the caller wants us to create a brand new empty data store...
    //
    if ( createNew )
    {
        unlink ( SYS_SHARED_MEMORY_SEGMENT_NAME );
    }
    //
    //  Open the shared memory segment file and verify that it opened
    //  properly.
    //
    int fd = 0;
    fd = open ( SYS_SHARED_MEMORY_SEGMENT_NAME, O_RDWR|O_CREAT, 0666);
    if (fd < 0)
    {
        printf ( "Unable to open the VSI system data store[%s] errno: %u[%m].\n",
                 SYS_SHARED_MEMORY_SEGMENT_NAME, errno );
        return 0;
    }
    //
    //  Go get the size of the shared memory segment.
    //
    int         status;
    struct stat stats;

    status = fstat ( fd, &stats );
    if ( status == -1 )
    {
        printf ( "Unable to get the size of the VSI system data store[%s] errno: "
                 "%u[%m].\n", SYS_SHARED_MEMORY_SEGMENT_NAME, errno );
        (void) close ( fd );
        return 0;
    }
    if ( stats.st_size <= 0 )
    {
        LOG ( "VSI system data store[%s]\n", SYS_SHARED_MEMORY_SEGMENT_NAME );
        LOG ( "   is uninitialized - Initializing it...\n" );

        //
        //  Go initialize the shared memory segment.
        //
        sysControl = sm_initialize_sys ( fd, SYS_INITIAL_SHARED_MEMORY_SIZE );
        if ( sysControl == 0 )
        {
            printf ( "Unable to initialize the VSI system data store[%s] errno: "
                     "%u[%m].\n", SYS_SHARED_MEMORY_SEGMENT_NAME, errno );
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
        sysControl = mmap ( NULL, stats.st_size, PROT_READ|PROT_WRITE,
                           MAP_SHARED, fd, 0 );
        if ( sysControl == MAP_FAILED )
        {
            printf ( "Unable to map the VSI system data store. errno: %u[%m].\n",
                     errno );
            return 0;
        }
    }
    LOG ( "VSI system data store at %p for %'lu bytes has been mapped into "
          "memory\n", sysControl, sysControl->sharedMemorySegmentSize );
    //
    //  Close the shared memory pseudo file as we don't need it any more.
    //
    (void) close ( fd );

    //
    //  Return the address of the memory mapped shared memory segment to the
    //  caller.
    //
    return sysControl;
}


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ c l o s e

    @brief Close the shared memory segment.

    This function will unmap the shared memory segment from memory, making it
    no longer accessable.  Note that this call will cause the data that has
    been modified in memory to be synchronized to the disk file that stores
    the data persistently.

    @return None

------------------------------------------------------------------------*/
void vsi_core_close ( void )
{
    //
    //  TODO: This crashes the system on exit - the call returns 0...
    //
    vsi_core_close_user();
    vsi_core_close_sys();

    return;
}


static void vsi_core_close_user ( void )
{
    int status;

    LOG ( "VSI user pages at %p for %'lu bytes being closed and unmapped\n", smControl,
          smControl->sharedMemorySegmentSize );
    //
    //  Close the user shared memory segment.
    //
    status = munmap ( smControl, smControl->sharedMemorySegmentSize );

    if ( status != 0 )
    {
        printf ( "Error: Cannot unmap user pages at %p for %lu bytes - "
                 "%s[%d]\n", smControl, smControl->sharedMemorySegmentSize,
                 strerror(errno), errno );
    }
    return;
}


static void vsi_core_close_sys ( void )
{
    int status;

    LOG ( "VSI system pages at %p for %'lu bytes being closed and unmapped\n",
          sysControl, sysControl->sharedMemorySegmentSize );
    //
    //  Close the system shared memory segment.
    //
    status = munmap ( sysControl, sysControl->sharedMemorySegmentSize );

    if ( status != 0 )
    {
        printf ( "Error: Cannot unmap system pages at %p for %lu bytes - "
                 "%s[%d]\n", sysControl, sysControl->sharedMemorySegmentSize,
                 strerror(errno), errno );
    }
    return;
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

    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

    @return None

------------------------------------------------------------------------*/
void vsi_core_insert ( domain_t domain, offset_t key, unsigned long newMessageSize,
                       void* body )
{
    //
    //  Go insert this key into the core data store.
    //
    sm_insert ( domain, key, newMessageSize, body );

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
int vsi_core_fetch_wait ( domain_t domain,
                          offset_t key,
                          unsigned long* bodySize,
                          void* body )
{
    return sm_fetch ( domain, key, bodySize, body, true );
}


int vsi_core_fetch ( domain_t domain,
                     offset_t key,
                     unsigned long* bodySize,
                     void* body )
{
    return sm_fetch ( domain, key, bodySize, body, false );
}


int vsi_core_fetch_newest ( domain_t domain,
                            offset_t key,
                            unsigned long* bodySize,
                            void* body )
{
    return sm_fetch_newest ( domain, key, bodySize, body, true );
}


int vsi_core_flush_signal ( domain_t domain,
                            offset_t key )
{
    return sm_flush_signal ( domain, key );
}


/*! @} */

// vim:filetype=c:syntax=c
