/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file lua_vsi_core.c

    This file contains the Lua to C interface functions that allow Lua
    programs to call the VSI Core functions.

-----------------------------------------------------------------------------*/

#ifndef LUA_VSI_CORE_API_H
#define LUA_VSI_CORE_API_H


#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "sharedMemory.h"
#include "utils.h"
#include "vsi_core_api.h"


/*! @{ */


/*!-----------------------------------------------------------------------

    v s i _ c o r e _ h a n d l e

    @brief  Define the handle to the VSI core data.

    From the Lua interface, this core handle is managed by the interface
    routines themselves and the user does not need to worry about supplying
    the handle other than having to "open" and "close" the core data store.

    The VSI core handle is an opaque object that is created by opening the VSI
    core.  That handle is then passed to any of the VSI core functions that
    need to be called.

------------------------------------------------------------------------*/
typedef void* vsi_core_handle;

//
//  Define our local persistent handle object.
//
static vsi_core_handle handle = 0;


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e O p e n

    @brief Open the VSI core environment.

    Lua Interface:

      Input arguments:  None

      Return values:
        handle - a lightuserdata type or NIL if an error occurs.
        message - a string type indicating the error or NIL if no error.

    If the VSI core environment does not exist yet, it will be created by this
    call.

    VSI C Interface:

        vsi_core_handle vsi_core_open ( void );

	@param  None

	@return The handle to the VSI core data store.
            NULL indicates an error.

------------------------------------------------------------------------*/
static int Lua_vsiCoreOpen ( lua_State* L )
{
    //
    //  Go open the VSI core data store.
    //
    handle = vsi_core_open();

    //
    //  If there was an error, return a NIL value to the caller along with the
    //  error message string.
    //
    if ( handle == NULL )
    {
        lua_pushnil ( L );
        lua_pushstring ( L, "Unable to open VSI Core" );
    }
    //
    //  If the call was successful, return the VSI core handle value to the
    //  caller alone with a NIL value for the error message string.
    //
    else
    {
        lua_pushlightuserdata ( L, handle );
        lua_pushnil ( L );
    }
    //
    //  Indicate that we are returning 2 fields to Lua.
    //
    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e C l o s e

	@brief Close the VSI core environment.

    Lua Interface:

      Input arguments:  None

      Return values:  None

    Note that this call will undo everything that was set up by the "open"
    call.  However, it will not destroy the persistent data created while the
    core was in use.  Only the data allocated in the context of the virtual
    address space of the current process will be destroyed.

    After calling this function, any access to an item located in the core
    data store will result in a SEGV signal being generated.

    VSI C Interface:

        void vsi_core_close ( vsi_core_handle handle );

	@param[in] handle - The VSI data store handle.

	@return None

------------------------------------------------------------------------*/
static int Lua_vsiCoreClose ( lua_State* L )
{
    //
    //  Go close the VSI core data store.
    //
    vsi_core_close ( handle );

    //
    //  Mark the VSI core data store as "closed".
    //
    handle = 0;

    //
    //  Indicate that we are not returning anything to Lua.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e I n s e r t

	@brief Insert a message into the VSI core data store.

    Lua Interface:

      Input arguments:
        domain - a number indicating what type of data this is (e.g. CAN, etc.).
        key - a number identifying a specific signal in the domain.
        value - the numeric (8 byte) value to be stored.

      Return values:  None

    This function will insert a new message into the VSI core data store with
    the given domain and key values.  If there is no space left in the data
    sore for this domain and key, the oldest record(s) in the data store for
    that domain and key will be overwritten with this new data.  In this way,
    the newest data items will always be in the data store.

    VSI C Interface:

        void vsi_core_insert ( vsi_core_handle handle,
                               domains domain,
                               offset_t key,
                               unsigned long newMessageSize,
                               void* body );

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

	@return None

------------------------------------------------------------------------*/
static int Lua_vsiCoreInsert ( lua_State* L )
{
    unsigned int  domain = lua_tonumber ( L, 1 );
    unsigned int  key    = lua_tonumber ( L, 2 );
    unsigned long value  = lua_tonumber ( L, 3 );

    vsi_core_insert ( handle, domain, key, 8, &value );

    return 0;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e F e t c h

	@brief Fetch and remove the oldest message from the VSI data store.

    Lua Interface:

      Input arguments:
        domain - a number indicating what type of data this is (e.g. CAN, etc.).
        key - a number identifying a specific signal in the domain.

      Return values:
        value - the numeric (8 byte) value fetched from the data store.
        status - the completion status of the operation (0 == successful).

    This function will find the oldest message with the specified domain and
    key values in the VSI data store, return the message data to the caller
    and delete the message from the VSI data store.  If the data requested is
    not available in the data store, this function will return immediately
    with an error code.

    VSI C Interface:

        int vsi_core_fetch ( vsi_core_handle handle, domains domain,
                             offset_t key, unsigned long* bodySize,
                             void* body );

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of the user's message buffer.

	@return 0      - Success
            ENOMSG - The requested domain/key does not exist.
                   - Anything else is an error code.

------------------------------------------------------------------------*/
static int Lua_vsiCoreFetch ( lua_State* L )
{
    unsigned long value = 0;
    unsigned long size  = 8;

    unsigned int  domain = lua_tonumber ( L, 1 );
    unsigned int  key    = lua_tonumber ( L, 2 );

    int status = vsi_core_fetch ( handle, domain, key, &size, &value );

    lua_pushnumber ( L, value );
    lua_pushnumber ( L, status );

    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e F e t c h W a i t

	@brief Fetch and remove a message from the VSI data store with wait.

    Lua Interface:

      Input arguments:
        domain - a number indicating what type of data this is (e.g. CAN, etc.).
        key - a number identifying a specific signal in the domain.

      Return values:
        value - the numeric (8 byte) value to be stored.
        status - the completion status of the operation (0 == successful).

    This function will find the message with the specified domain and key
    values in the VSI data store, return the message data to the caller and
    delete the message from the VSI data store.  If the data requested is not
    available in the data store, this function will wait indefinitely and only
    return when the data requested is available.

    This function is identical to the vsi_core_fetch function except for the
    wait behavior.

    VSI C Interface:

        int vsi_core_fetch_wait ( vsi_core_handle handle, domains domain,
                                  offset_t key, unsigned long* bodySize,
                                  void* body );

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of the user's message buffer.

	@return 0 - Success
              - Anything else is an error code.

------------------------------------------------------------------------*/
static int Lua_vsiCoreFetchWait ( lua_State* L )
{
    unsigned long value = 0;
    unsigned long size  = 8;

    unsigned int  domain = lua_tonumber ( L, 1 );
    unsigned int  key    = lua_tonumber ( L, 2 );

    int status = vsi_core_fetch_wait ( handle, domain, key, &size, &value );

    lua_pushnumber ( L, value );
    lua_pushnumber ( L, status );

    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e F e t c h N e w e s t

	@brief Fetch the newest message from the VSI data store with wait.

    Lua Interface:

      Input arguments:
        domain - a number indicating what type of data this is (e.g. CAN, etc.).
        key - a number identifying a specific signal in the domain.

      Return values:
        value - the numeric (8 byte) value to be stored.
        status - the completion status of the operation (0 == successful).

    This function will find the message with the specified domain and key
    This function will find the newest message with the specified domain and
    key values in the VSI data store and return the message data to the
    caller.  If the data requested is not available in the data store, this
    function will wait indefinitely and only return when the data requested is
    available.

    VSI C Interface:

        int vsi_core_fetch_newest ( vsi_core_handle handle, domains domain,
                                    offset_t key, unsigned long* bodySize,
                                    void* body );

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of the user's message buffer.

	@return 0 - Success
              - Anything else is an error code.

------------------------------------------------------------------------*/
static int Lua_vsiCoreFetchNewest ( lua_State* L )
{
    unsigned long value = 0;
    unsigned long size  = 8;

    unsigned int  domain = lua_tonumber ( L, 1 );
    unsigned int  key    = lua_tonumber ( L, 2 );

    int status = vsi_core_fetch_newest ( handle, domain, key, &size, &value );

    lua_pushnumber ( L, value );
    lua_pushnumber ( L, status );

    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i C o r e F l u s h S i g n a l

	@brief Flush all instances of the specified signal from the data store.

    Lua Interface:

      Input arguments:
        domain - a number indicating what type of data this is (e.g. CAN, etc.).
        key - a number identifying a specific signal in the domain.

      Return values:
        status - the completion status of the operation (0 == successful).

    This function will find the message with the specified domain and key
	This function will find all of the instances of signals with the specified
    domain and key and remove them from the data store.  This call is designed
    to allow users to basically reset all of the data associated with a signal
    so the app can start from a known clean point.

        int vsi_core_flush_signal ( vsi_core_handle handle, domains domain,
                                    offset_t key );

    @param[in] handle - The handle to the VSI core data store.
    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.

	@return 0 on Success
            Anything else is an error code (errno value)

------------------------------------------------------------------------*/
static int Lua_vsiCoreFlushSignal ( lua_State* L )
{
    unsigned int domain = lua_tonumber ( L, 1 );
    unsigned int key    = lua_tonumber ( L, 2 );

    int status = vsi_core_flush_signal ( handle, domain, key );

    lua_pushnumber ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    l u a o p e n _ l i b l u a _ v s i _ c o r e

	@brief Register the Lua functions we have implemented here.

	This function will make calls to Lua to register the names of all of the
    functions that have been implemented in this library.

------------------------------------------------------------------------*/
int luaopen_liblua_vsi_core ( lua_State* L )
{
    lua_register ( L, "Lua_vsiCoreOpen", Lua_vsiCoreOpen );
    lua_register ( L, "Lua_vsiCoreClose", Lua_vsiCoreClose );
    lua_register ( L, "Lua_vsiCoreInsert", Lua_vsiCoreInsert );
    lua_register ( L, "Lua_vsiCoreFetch", Lua_vsiCoreFetch );
    lua_register ( L, "Lua_vsiCoreFetchWait", Lua_vsiCoreFetchWait );
    lua_register ( L, "Lua_vsiCoreFetchNewest", Lua_vsiCoreFetchNewest );
    lua_register ( L, "Lua_vsiCoreFlushSignal", Lua_vsiCoreFlushSignal );

    return 0;
}


#endif  // End of #ifndef LUA_VSI_CORE_API_H

/*! @} */

// vim:filetype=h:syntax=c
