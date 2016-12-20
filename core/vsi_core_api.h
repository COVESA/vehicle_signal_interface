/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file vsi_core_api.h

    This file contains the declarations of the Vechicle Signal Interface core
    module user API structures and functions.

-----------------------------------------------------------------------------*/

#ifndef VSI_CORE_API_H
#define VSI_CORE_API_H

#include "sharedMemory.h"
#include "utils.h"


/*! @{ */


/*!-----------------------------------------------------------------------

    v s i C o r e H a n d l e

    @brief  Define the handle to the VSI core data.

    The VSI core handle is an opaque object that is created by opening the VSI
    core.  That handle is then passed to any of the VSI core functions that
    need to be called.

------------------------------------------------------------------------*/
typedef void* vsi_core_handle;


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ o p e n

    @brief Open the VSI VSI core environment.

    If the VSI core environment does not exist yet, it will be created by this
    call.

	@param[in] - createNew - Create a brand new empty data store.

	@return The handle to the VSI core data store.
            NULL indicates an error.

------------------------------------------------------------------------*/
void vsi_core_open ( bool createNew );


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ c l o s e

	@brief Close the VSI core environment.

    Note that this call will undo everything that was set up by the "open"
    call.  However, it will not destroy the persistent data created while the
    core was in use.  Only the data allocated in the context of the virtual
    address space of the current process will be destroyed.

    After calling this function, any access to an item located in the core
    data store will result in a SEGV signal being generated.

	@return None

------------------------------------------------------------------------*/
void vsi_core_close ( void );


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ i n s e r t

	@brief Insert a message into the VSI core data store.

    This function will insert a new message into the VSI core data store with
    the given domain and key values.  If there is no space left in the data
    sore for this domain and key, the oldest record(s) in the data store for
    that domain and key will be overwritten with this new data.  In this way,
    the newest data items will always be in the data store.

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

	@return None

------------------------------------------------------------------------*/
void vsi_core_insert ( domain_t domain,
                       offset_t key, unsigned long newMessageSize,
                       void* body );

/*!-----------------------------------------------------------------------

	v s i _ c o r e _ f e t c h

	@brief Fetch and remove the oldest message from the VSI data store.

    This function will find the oldest message with the specified domain and
    key values in the VSI data store, return the message data to the caller
    and delete the message from the VSI data store.  If the data requested is
    not available in the data store, this function will return immediately
    with an error code.

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of the user's message buffer.

	@return 0      - Success
            ENOMSG - The requested domain/key does not exist.
                   - Anything else is an error code.

------------------------------------------------------------------------*/
int vsi_core_fetch ( domain_t domain,
                     offset_t key, unsigned long* bodySize,
                     void* body );

/*!-----------------------------------------------------------------------

    v s i _ c o r e _ f e t c h _ w a i t

	@brief Fetch and remove a message from the VSI data store with wait.

    This function will find the message with the specified domain and key
    values in the VSI data store, return the message data to the caller and
    delete the message from the VSI data store.  If the data requested is not
    available in the data store, this function will wait indefinitely and only
    return when the data requested is available.

    This function is identical to the vsi_core_fetch function except for the
    wait behavior.

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of the user's message buffer.

	@return 0 - Success
              - Anything else is an error code.

------------------------------------------------------------------------*/
int vsi_core_fetch_wait ( domain_t domain,
                          offset_t key, unsigned long* bodySize,
                          void* body );


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ f e t c h _ n e w e s t

	@brief Fetch the newest message from the VSI data store with wait.

    This function will find the newest message with the specified domain and
    key values in the VSI data store and return the message data to the
    caller.  If the data requested is not available in the data store, this
    function will wait indefinitely and only return when the data requested is
    available.

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of the user's message buffer.

	@return 0 - Success
              - Anything else is an error code.

------------------------------------------------------------------------*/
int vsi_core_fetch_newest ( domain_t domain,
                            offset_t key, unsigned long* bodySize,
                            void* body );


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ f l u s h _ s i g n a l

	@brief Flush all instances of the specified signal from the data store.

	This function will find all of the instances of signals with the specified
    domain and key and remove them from the data store.  This call is designed
    to allow users to basically reset all of the data associated with a signal
    so the app can start from a known clean point.

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.

	@return 0 on Success
            Anything else is an error code (errno value)

------------------------------------------------------------------------*/
int vsi_core_flush_signal ( domain_t domain,
                            offset_t key );


#endif  // End of #ifndef VSI_CORE_API_H

/*! @} */

// vim:filetype=h:syntax=c
