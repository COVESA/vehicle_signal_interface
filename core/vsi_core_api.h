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


//
//  Open the VSI VSI core environment.
//
//  If the VSI core environment does not exist yet, it will be created by this call.
//
vsi_core_handle vsi_core_open ( void );


//
//  Close the VSI core environment.
//
//  Note that this call will undo everything that was set up by the "open"
//  call.  However, it will not destroy the persistent data created while the
//  core was in use.  Only the data allocated in the context of the virtual
//  address space of the current process will be destroyed.
//
//  After calling this function, any access to an item located in the core
//  data store will result in a SEGV signal being generated.
//
void vsi_core_close ( vsi_core_handle vsiCore );


//
//  Insert a message into the VSI core data store.
//
//  This function will insert a new message into the VSI core data store with
//  the given domain and key values.  If there is no space left in the data
//  sore for this domain and key, the oldest record(s) in the data store for
//  that domain and key will be overwritten with this new data.  In this way,
//  the newest data items will always be in the data store.
//
void vsi_core_insert ( vsi_core_handle vsiCore, offset_t key,
                       enum domains domain, unsigned long newMessageSize,
                       void* body );
//
//  Fetch and remove a message from the VSI data store.
//
//  This function will find the message with the specified domain and key
//  values in the VSI data store, return the message data to the caller and
//  delete the message from the VSI data store.
//
int vsi_core_fetch ( vsi_core_handle vsiCore, offset_t key,
                     enum domains domain, unsigned long newMessageSize,
                     void* body );

int vsi_core_fetch_wait ( vsi_core_handle vsiCore, offset_t key,
                          enum domains domain, unsigned long newMessageSize,
                          void* body );


#endif  // End of #ifndef VSI_CORE_API_H

/*! @} */

// vim:filetype=h:syntax=c
