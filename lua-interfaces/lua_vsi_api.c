/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file lua_vsi_api.c

    This file contains the Lua to C interface functions that allow Lua
    programs to call the VSI API functions.

-----------------------------------------------------------------------------*/

#ifndef LUA_VSI_API_H
#define LUA_VSI_API_H


#include <lua.h>
#include <lauxlib.h>
#include <errno.h>
#include <string.h>

#define VSI_DEBUG

#include "vsi.h"


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
//
//  Define our local persistent handle object.
//
static vsi_core_handle handle = 0;


/*!-----------------------------------------------------------------------

    S t a r t u p   a n d   S h u t d o w n
    =======================================


    L u a _ v s i _ i n i t i a l i z e

    @brief Initialize the VSI API.

    Lua Interface:

      handle, message = Lua_vsi_initialize ()

        Input arguments: None

        Return values:
            handle - a lightuserdata type or NIL if an error occurs.
            message - a string type indicating the error or NIL if no error.

    This function must be the first function called by applications interested
    in vehicle signals.  The returned vsi_handle object must be supplied as an
    argument to all of the VSI API functions.

    If this function fails for some reason, the returned value will be NULL
    and errno will reflect the error that occurred.

    VSI API C Interface:

        vsi_handle vsi_initialize ();

    @param[in] None

    @return vsi_handle - The VSI handle object representing this VSI context.

------------------------------------------------------------------------*/
static int Lua_vsi_initialize ( lua_State* L )
{
    bool createNew = false;

    //
    //  Get the first argument as a boolean.
    //
    if ( lua_gettop ( L ) != 0 )
    {
        createNew = lua_toboolean ( L, 1 );
    }
    LOG ( "\nlua_vsi_api[%d] is initializing the Lua interface library...\n",
          createNew );
    //
    //  Go initialize the VSI API library.  Note that this call will also
    //  initialize the VSI Core module.
    //
    handle = vsi_initialize ( createNew );

    //
    //  If there was an error, return a NIL value to the caller along with the
    //  error message string.
    //
    if ( handle == NULL )
    {
        lua_pushnil ( L );
        lua_pushstring ( L, "Unable to open VSI API library" );
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

    L u a _ v s i _ d e s t r o y

    @brief Destroy an existing VSI context.

    Lua Interface:

      Lua_vsi_destroy ()

        Input arguments:  None

        Return values:  None

    This function will clean up and deallocate all resources utilized by the
    VSI API and it should always be called once the VSI API is no longer
    required.  Once this function is called, the vsi_handle will no longer be
    valid and utilizing it or calling any of the VSI API functions will result
    in undefined behavior.

    VSI API C Interface:

        int vsi_destroy ( void );

    @param[in] - handle - The VSI context handle

    @return - status - The function return status value.

------------------------------------------------------------------------*/
static int Lua_vsi_destroy ( lua_State* L )
{
    //
    //  Go destroy the current VSI API environment.
    //
    (void) vsi_destroy ( handle );

    //
    //  Indicate that we are not returning anything to the caller.
    //
    return 0;
}


/*!----------------------------------------------------------------------------

    L u a _ v s i _ V S S _ i m p o r t

	@brief Import a VSS file into the VSI data store.

    Lua Interface:

      status = Lua_vsi_VSS_import ( "/somePath/vss_rel_1.vsi" )

        Input arguments:
            fileName - A string representing the name and location of the
                       input file.

        Return values:
            Completion code (number)

	This function will read the specified file and import the contents of it
    into the specified VSI environment.

    VSI API C Interface:

        int vsi_VSS_import ( vsi_handle handle, const char* fileName );

	@param[in] - handle - The VSI context handle
	@param[in] - fileName - The pathname to the VSS definition file to be read

	@return - Completion code - 0 = Succesful
                                Anything else is an error code

-----------------------------------------------------------------------------*/
static int Lua_vsi_VSS_import ( lua_State* L )
{
    const char* fileName = luaL_checkstring ( L, 1 );

    int status = vsi_VSS_import ( handle, fileName );

    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G e n e r a t i o n
    =========================================


    L u a _ v s i _ f i r e _ s i g n a l

    @brief Fire a vehicle signal by its ID.

    Lua Interface:

      status = Lua_vsi_fire_signal ( domain, signal, value )

    or:

      status = Lua_vsi_fire_signal ( name, value )

        Input arguments:
            domain - An integer domain ID.
            signal - An integer signal ID.
            value  - An integer data value for this signal.

          or:
            name - A signal name string.
            value  - An integer data value for this signal.

        Return values:
            status - The integer completion status of the function.

    These functions will allow an application to generate a new signal into
    the VSI database.  The signal can be specified with either it's name or
    its domain and ID.  If the name form is used, the specified name must be
    defined in the VSS specification for this signal set, otherwise, an error
    will be generated.

    These calls will copy the supplied information into the VSI database and
    if another thread or process is waiting on that signal, processing will
    resume for that thread or process.  This call will return to the caller
    immediately under all circumstances with the return value indicating the
    status of the operation.

    The following fields must be set in the result data structure:

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The data length parameter is the size of the user's data buffer being
    supplied.  This is the maximum number of bytes of data that will be copied
    into the supplied buffer by this code.  This value should be set to the
    maximum size of the data buffer being supplied.  If the data buffer
    supplied is not large enough to contain the entire data record, the data
    will be truncated to fix into the buffer.

    The data pointer is the address of the buffer in which the record data
    will be copied.  Note that the data is copied as binary data and will not
    automatically have a null appended to it if it is actually ASCII data.  It
    is the caller's resonsibility to ensure that if the data being retrieved
    is ASCII data, that those strings are properly null terminated.

    TODO: What happens if the specified signal does not exist yet?

    This function returns a 0 upon success and an errno code otherwise.

    VSI API C Interface:

        int vsi_fire_signal ( vsi_result* result );

    @param[in] - result - The vsi_result object that will contain the
                          information needed to fire the signal.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_fire_signal ( lua_State* L )
{
    vsi_result    result;
    int           status = 0;
    unsigned long userData;

    LOG ( "In: Lua_vsi_fire_signal\n" );

    //
    //  Initialize the result object.
    //
    result.context    = handle;
    result.domainId   = 0;
    result.signalId   = 0;
    result.name       = NULL;
    result.data       = (char*)&userData;
    result.dataLength = 8;
    result.status     = 0;

    //
    //  If the first input argument is a string, assume it is the name of the
    //  signal we should generate.
    //
    if ( lua_type ( L, 1 ) == LUA_TSTRING )
    {
        result.name = (char*)lua_tostring ( L, 1 );
        userData    = luaL_checkinteger ( L, 2 );

        LOG ( "  Firing signal by name: %s with value: %lu\n", result.name,
              userData );

        status = vsi_fire_signal_by_name ( handle, &result );
    }
    //
    //  If the first argument is not a string, then it must be a domain Id
    //  followed by a signal Id.
    //
    else
    {
        result.domainId = luaL_checkinteger ( L, 1 );
        result.signalId = luaL_checkinteger ( L, 2 );
        userData        = luaL_checkinteger ( L, 3 );

        LOG ( "  Firing signal by ID: %d,%d with value: %lu\n", result.domainId,
              result.signalId, userData );

        status = vsi_fire_signal ( handle, &result );
    }
    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   R e t r i e v a l
    =======================================

    L u a _ v s i _ g e t _ o l d e s t _ s i g n a l

    @brief Fetch the oldest entry in the core database for a signal.

    Lua Interface:

      status, value, name = Lua_vsi_get_oldest_signal ( domain, signal )

    or:

      status, value, name = Lua_vsi_get_oldest_signal ( name )

        Input arguments:
            domain - An integer domain ID.
            signal - An integer signal ID.

          or:
            name - A signal name string.

        Return values:
            status - The integer completion status of the function.
            value  - An integer data value for this signal.
            name  - The signal name string.

    This function will find the oldest entry in the core database for the
    specified signal domain and ID and return the data associated with that
    signal to the caller.  The signal data returned will be removed from the
    core database and will no longer be available to anyone.

    The calling application is responsible for providing a vsi_result structure
    as input and for providing both a pointer to a valid data buffer and the
    length of the buffer inside that structure as information in the
    structure. The API will write data to the buffer and overwrite the length
    field with the number of bytes written. If the API detects an error
    specific to the signal, it will log an error in the status field of the
    structure.

    If two or more signals have been generated since the last read, the oldest
    signal will be read first. Once the oldest signal has been read, the
    message will be removed from the core database, thereby "consumming" the
    data.  Calling applications may continue to consume data by issuing
    additional calls to vsi_get_oldest_signal until they reach the end of the
    buffer, where an error will be returned to indicate that there is no more
    data.

    The following fields must be supplied in the result data structure:

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The calling application is responsible for providing a vsi_result structure
    as input and for providing both a pointer to a valid data buffer and the
    length of the buffer as information in the structure. The API will write
    data to the buffer and overwrite the length field with the number of bytes
    written into the buffer. If the API detects an error specific to the
    signal, it will log an error in the status field of the structure.

    VSI API C Interface:

        int vsi_get_oldest_signal ( vsi_result* result );

    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_get_oldest_signal ( lua_State* L )
{
    vsi_result    result;
    int           status = 0;
    unsigned long userData;

    LOG ( "In: Lua_vsi_get_oldest_signal\n" );
    LOG ( "      Stack top is %d\n", lua_gettop ( L ) );

    //
    //  Initialize the result object.
    //
    result.context    = handle;
    result.domainId   = 0;
    result.signalId   = 0;
    result.name       = NULL;
    result.data       = (char*)&userData;
    result.dataLength = 8;
    result.status     = 0;

    //
    //  If the first input argument is a string, assume it is the name of the
    //  signal we should generate.
    //
    if ( lua_type ( L, 1 ) == LUA_TSTRING )
    {
        result.name = (char*)lua_tostring ( L, 1 );

        LOG ( "  Calling vsi_get_oldest_signal_by_name[%s]\n", result.name );

        status = vsi_get_oldest_signal_by_name ( handle, &result );
    }
    //
    //  If the first argument is not a string, then it must be a domain Id
    //  followed by a signal Id.
    //
    else
    {
        result.domainId = luaL_checkinteger ( L, 1 );
        result.signalId = luaL_checkinteger ( L, 2 );

        LOG ( "  Calling vsi_get_oldest_signal[%d,%d]\n", result.domainId,
              result.signalId );

        status = vsi_get_oldest_signal ( handle, &result );
    }
    //
    //  Return the status code to the caller.
    //
    LOG ( "  vsi_get_oldest_signal returning: %lu\n", userData );

    lua_pushinteger ( L, status );
    lua_pushinteger ( L, userData );
    lua_pushstring ( L, result.name );

    return 3;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ g e t _ n e w e s t _ s i g n a l

    @brief Fetch the latest value of the specified signal by ID.

    Lua Interface:

      status, value, name = Lua_vsi_get_newest_signal ( domain, signal )

    or:

      status, value, name = Lua_vsi_get_newest_signal ( name )

        Input arguments:
            domain - An integer domain ID.
            signal - An integer signal ID.

          or:
            name - A signal name string.

        Return values:
            status - The integer completion status of the function.
            value  - An integer data value for this signal.
            name  - The signal name string.

    These functions will retrieve the latest signal of the specified domain
    and ID from the core database.  If no signal of the specified type is
    available when the call is made, the functions will suspend execution
    until a signal is available.  At that point, the information for that
    signal will be returned to the caller in the result object.

    Unlike the vsi_get_oldest_signal function, this call does not remove a
    message from the core database.

    The following fields must be supplied in the result data structure:

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The calling application is responsible for providing a vsi_result structure
    as input and for providing both a pointer to a valid data buffer and the
    length of the buffer as information in the structure. The API will write
    data to the buffer and overwrite the length field with the number of bytes
    written into the buffer. If the API detects an error specific to the
    signal, it will log an error in the status field of the structure.

    VSI API C Interface:

        int vsi_get_newest_signal ( vsi_handle  handle, vsi_result* result );

    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_get_newest_signal ( lua_State* L )
{
    vsi_result    result;
    int           status = 0;
    unsigned long userData;

    LOG ( "In: Lua_vsi_get_newest_signal\n" );
    LOG ( "      Stack top is %d\n", lua_gettop ( L ) );

    //
    //  Initialize the result object.
    //
    result.context    = handle;
    result.domainId   = 0;
    result.signalId   = 0;
    result.name       = NULL;
    result.data       = (char*)&userData;
    result.dataLength = 8;
    result.status     = 0;

    //
    //  If the first input argument is a string, assume it is the name of the
    //  signal we should generate.
    //
    if ( lua_type ( L, 1 ) == LUA_TSTRING )
    {
        result.name = (char*)lua_tostring ( L, 1 );

        LOG ( "  Calling vsi_get_newest_signal_by_name[%s]\n", result.name );

        status = vsi_get_newest_signal_by_name ( handle, &result );
    }
    //
    //  If the first argument is not a string, then it must be a domain Id
    //  followed by a signal Id.
    //
    else
    {
        result.domainId = luaL_checkinteger ( L, 1 );
        result.signalId = luaL_checkinteger ( L, 2 );

        LOG ( "  Calling vsi_get_newest_signal[%d,%d]\n", result.domainId,
              result.signalId );

        status = vsi_get_newest_signal ( handle, &result );
    }
    //
    //  Return the status code to the caller.
    //
    LOG ( "  vsi_get_newest_signal returning: %lu\n", userData );

    lua_pushinteger ( L, status );
    lua_pushinteger ( L, userData );
    lua_pushstring ( L, result.name );

    return 3;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ f l u s h _ s i g n a l

    @brief Flush all signals by domain and ID.

    Lua Interface:

      status = Lua_vsi_flush_signal ( domain, signal )

    or:

      status = Lua_vsi_flush_signal ( name )

        Input arguments:
            domain - An integer domain ID.
            signal - An integer signal ID.

          or:
            name - A signal name string.

        Return values:
            The completion status of the function.

    This function flushes all unread messages from the VSI core database for
    the signal with the specified domain and ID.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the core database.

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    VSI API C Interface:

        int vsi_flush_signal ( vsi_handle handle, const domain_t domainId,
                               const signal_t signalId );

    @param[in] - domainId - The domain ID.
    @param[in] - signalId - The signal ID.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_flush_signal ( lua_State* L )
{
    vsi_result result;
    int        status = 0;

    //
    //  Initialize the result object.
    //
    result.context    = handle;
    result.domainId   = 0;
    result.signalId   = 0;
    result.name       = NULL;
    result.data       = 0;
    result.dataLength = 8;
    result.status     = 0;

    //
    //  If the first input argument is a string, assume it is the name of the
    //  signal we should flush.
    //
    if ( lua_type ( L, 1 ) == LUA_TSTRING )
    {
        result.name = (char*)lua_tostring ( L, 1 );

        status = vsi_flush_signal_by_name ( handle, result.name );
    }
    //
    //  If the first argument is not a string, then it must be a domain Id
    //  followed by a signal Id.
    //
    else
    {
        result.domainId = luaL_checkinteger ( L, 1 );
        result.signalId = luaL_checkinteger ( L, 2 );

        status = vsi_flush_signal ( handle, result.domainId, result.signalId );
    }
    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G r o u p   F u n c t i o n s
    ===================================================


    L u a _ v s i _ c r e a t e _ s i g n a l _ g r o u p

    @brief Create a new signal group.

    Lua Interface:

      status = Lua_vsi_create_signal_group ( groupId )

        Input arguments:
            groupId - The desired integer group ID.

        Return values:
            The completion status of the function.

    This function will create a new signal group with the specified ID value.
    Note that groups are not visible to other tasks connected to the VSI
    module.

    The groupId is a numeric value that the caller wants to assign to this
    group.  The value can be anything that is not currently in use within the
    possible range of value for this data type.

    VSI API C Interface:

        int vsi_create_signal_group ( vsi_handle    handle,
                                      const group_t groupId );

    @param[in] - groupId - The ID value desired for this new group.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_create_signal_group ( lua_State* L )
{
    int status = 0;

    unsigned int groupId = luaL_checkinteger ( L, 1 );

    status = vsi_create_signal_group ( handle, groupId );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ d e l e t e _ s i g n a l _ g r o u p

    @brief Delete the specified signal group from the system.

    Lua Interface:

      status = Lua_vsi_delete_signal_group ( groupId )

        Input arguments:
            groupId - The group to be deleted.

        Return values:
            The completion status of the function.

    This function will delete the specified signal group from the system.

    VSI API C Interface:

        int vsi_delete_signal_group ( vsi_handle    handle,
                                      const group_t groupId);

    @param[in] - groupId - The ID value of the group to be deleted.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_delete_signal_group ( lua_State* L )
{
    int status = 0;

    unsigned int groupId = luaL_checkinteger ( L, 1 );

    status = vsi_delete_signal_group ( handle, groupId );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ a d d _ s i g n a l _ t o _ g r o u p

    @brief Add a new signal to the specified group.

    Lua Interface:

      status = Lua_vsi_add_signal_to_group ( domain, signal, group )

    or:

      status = Lua_vsi_add_signal_to_group ( name, group )

        Input arguments:
            domain - An integer domain ID.
            signal - An integer signal ID.
            group  - An integer group ID.

          or:
            name - A signal name string.
            group  - An integer group ID.

        Return values:
            The completion status of the function.

    This function will add a new signal to the specified group.

    The domainId is an identifier for which domain this record should be
    located in.  This is currently defined as a small positive integer value.

    The signalId is a value which will identify the specific signal desired by
    the caller within the domain specified.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    VSI API C Interface:

        int vsi_add_signal_to_group ( const domain_t domainId,
                                      const signal_t signalId,
                                      const group_t  groupId );

    @param[in] - domainId - The domain ID value of the signal to be added.
    @param[in] - signalId - The signal ID value of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function
------------------------------------------------------------------------*/
static int Lua_vsi_add_signal_to_group ( lua_State* L )
{
    int status = 0;

    //
    //  If the first input argument is a string, assume it is the name of the
    //  signal we should add.
    //
    if ( lua_type ( L, 1 ) == LUA_TSTRING )
    {
        const char* name = luaL_checkstring ( L, 1 );
        int groupId      = luaL_checkinteger ( L, 2 );

        status = vsi_add_signal_to_group_by_name ( handle, name, groupId );
    }
    //
    //  If the first argument is not a string, then it must be a domain Id
    //  followed by a signal Id.
    //
    else
    {
        int domain  = luaL_checkinteger ( L, 1 );
        int signal  = luaL_checkinteger ( L, 2 );
        int groupId = luaL_checkinteger ( L, 3 );

        status = vsi_add_signal_to_group ( handle, domain, signal, groupId );
    }
    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p

    @brief Remove the specified signal from the given group.

    Lua Interface:

      status = Lua_vsi_remove_signal_from_group ( domain, signal, group )

    or:

      status = Lua_vsi_remove_signal_from_group ( name, group )

        Input arguments:
            domain - An integer domain ID.
            signal - An integer signal ID.
            group  - An integer group ID.

          or:
            name - A signal name string.
            group  - An integer group ID.

        Return values:
            The completion status of the function.

    This function will remove a signal, specified by it's domain and ID, and
    remove it from the specified group.

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a value which will identify the specific signal desired by
    the caller within the domain specified.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    VSI API C Interface:

        int vsi_remove_signal_from_group ( const domain_t domainId,
                                           const signal_t signalId,
                                           const group_t  groupId );

    @param[in] - domainId - The domain ID value of the signal to be added.
    @param[in] - signalId - The signal ID value of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_remove_signal_from_group ( lua_State* L )
{
    int status = 0;

    //
    //  If the first input argument is a string, assume it is the name of the
    //  signal we should remove.
    //
    if ( lua_type ( L, 1 ) == LUA_TSTRING )
    {
        const char*name = lua_tostring ( L, 1 );
        int groupId = lua_tointeger ( L, 2 );

        status = vsi_remove_signal_from_group_by_name ( handle, name, groupId );
    }
    //
    //  If the first argument is not a string, then it must be a domain Id
    //  followed by a signal Id.
    //
    else
    {
        int domain = luaL_checkinteger ( L, 1 );
        int signal = luaL_checkinteger ( L, 2 );
        int groupId = luaL_checkinteger ( L, 3 );

        status = vsi_remove_signal_from_group ( handle, domain, signal, groupId );
    }
    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ g e t _ n e w e s t _ i n _ g r o u p

    @brief Retrieve the newest signal for every member of the specified group.

    Lua Interface:

      status, results = Lua_vsi_get_newest_in_group ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.
            results - An array of results tables

        The "results" return value is an array of tables in which each table
        represents the results for one of the signals in the group and
        consists of:

            results.domainId   (integer)
            results.signalId   (integer)
            results.name       (string)
            results.data       (integer)
            results.dataLength (integer)
            results.status     (integer)

    The newest member of each signal in the specified group will be retrieved
    and stored in the result array.  If one or more of the signals do not have
    any members then the "ENODATA" status condition will be stored in the
    result structure for that particular signal.  Once all of the signals have
    been checked, this function returns to the caller without waiting.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures must
    be at least as large as the number of signals defined in the specified
    group.  Failure to allocate enough space for the return data will result
    in memory corruption and undefined behavior (almost always very bad!).

    The signal data that is retrieved and returned to the caller will NOT be
    removed from the VSI core database once it is copied into the result
    structures.

    VSI API C Interface:

        int vsi_get_newest_in_group ( vsi_handle    handle,
                                      const group_t groupId,
                                      vsi_result*   result );

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_get_newest_in_group ( lua_State* L )
{
    vsi_result*       results;
    vsi_signal_group  group;
    vsi_signal_group* temp = 0;
    int               status = 0;

    //
    //  Go get the group ID from the caller's input parameters.
    //
    group.groupId = luaL_checkinteger ( L, 1 );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        lua_pushinteger ( L, -ENOENT );
        lua_pushstring ( L, strerror(ENOENT) );
        return 2;
    }
    //
    //  Go allocate a buffer that will hold all of the possible results from
    //  the "get newest" call that follows.
    //
    results = malloc ( sizeof(vsi_result) * temp->count );

    //
    //  Go get all of the newest signals for each signal defined in this
    //  group.
    //
    status = vsi_get_newest_in_group ( handle, temp->groupId, results );

    //
    //  Create a new table that will be our array of result tables.
    //
    lua_newtable ( L );

    //
    //  For each result object in this group...
    //
    for ( int i = 0; i < temp->count; ++i )
    {
        //
        //  Create a new result table.
        //
        lua_newtable ( L );

        //
        //  Populate all of the fields in this new result table.
        //
        lua_pushstring ( L, "domain" );
        lua_pushinteger ( L, results[i].domainId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "signal" );
        lua_pushinteger ( L, results[i].signalId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "name" );
        lua_pushstring ( L, results[i].name );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "data" );
        lua_pushinteger ( L, (unsigned long)results[i].data );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "dataLength" );
        lua_pushinteger ( L, results[i].dataLength );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "status" );
        lua_pushinteger ( L, results[i].status );
        lua_settable ( L, -3 );

        //
        //  Create a new entry in the array with this result table.
        //
        lua_settable ( L, -4 );
    }
    //
    //  Give back the memory containing the results.
    //
    free ( results );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    //
    //  Return 2 items to the caller.
    //
    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ g e t _ n e w e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the newest signal for every member of the specified group.

    Lua Interface:

      status, results = Lua_vsi_get_newest_in_group_wait ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.
            results - An array of results tables

        The "results" return value is an array of tables in which each table
        represents the results for one of the signals in the group and
        consists of:

            results.domainId   (integer)
            results.signalId   (integer)
            results.name       (string)
            results.data       (integer)
            results.dataLength (integer)
            results.status     (integer)

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

    TODO: The body of this function is identical to the one above except for
    the VSI function that is called so it should be combined into a common
    function.

    VSI API C Interface:

        int vsi_get_newest_in_group_wait ( const group_t groupId,
                                           vsi_result*   result );

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_get_newest_in_group_wait ( lua_State* L )
{
    vsi_result*       results;
    vsi_signal_group  group;
    vsi_signal_group* temp = 0;

    //
    //  Go get the group ID from the caller's input parameters.
    //
    group.groupId = luaL_checkinteger ( L, 1 );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Go allocate a buffer that will hold all of the possible results from
    //  the "get newest" call that follows.
    //
    results = malloc ( sizeof(vsi_result) * temp->count );

    //
    //  Go get all of the newest signals for each signal defined in this
    //  group.
    //
    int status = vsi_get_newest_in_group_wait ( handle, temp->groupId, results );

    //
    //  Create a new table that will be our array of result tables.
    //
    lua_newtable ( L );

    //
    //  For each result object in this group...
    //
    for ( int i = 0; i < temp->count; ++i )
    {
        //
        //  Create a new result table.
        //
        lua_newtable ( L );

        //
        //  Populate all of the fields in this new result table.
        //
        lua_pushstring ( L, "domain" );
        lua_pushinteger ( L, results[i].domainId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "signal" );
        lua_pushinteger ( L, results[i].signalId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "name" );
        lua_pushstring ( L, results[i].name );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "data" );
        lua_pushinteger ( L, (unsigned long)results[i].data );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "dataLength" );
        lua_pushinteger ( L, results[i].dataLength );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "status" );
        lua_pushinteger ( L, results[i].status );
        lua_settable ( L, -3 );

        //
        //  Create a new entry in the array with this result table.
        //
        lua_settable ( L, -4 );
    }
    //
    //  Give back the memory containing the results.
    //
    free ( results );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    //
    //  Return 2 items to the caller.
    //
    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ g e t _ o l d e s t _ i n _ g r o u p

    @brief Retrieve the oldest signal for every member of the specified group.

    Lua Interface:

      status, results = Lua_vsi_get_oldest_in_group ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.
            results - An array of results tables

        The "results" return value is an array of tables in which each table
        represents the results for one of the signals in the group and
        consists of:

            results.domainId   (integer)
            results.signalId   (integer)
            results.name       (string)
            results.data       (integer)
            results.dataLength (integer)
            results.status     (integer)

    The oldest member of each signal in the specified group will be retrieved
    and stored in the result array.  If one or more of the signals do not have
    any members then the "ENODATA" status condition will be stored in the
    result structure for that particular signal.  Once all of the signals have
    been checked, this function returns to the caller without waiting.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures must
    be at least as large as the number of signals defined in the specified
    group.  Failure to allocate enough space for the return data will result
    in memory corruption and undefined behavior (almost always very bad!).

    The signal data that is retrieved and returned to the caller will be
    removed from the VSI core database once it is copied into the result
    structures

    VSI API C Interface:

        int vsi_get_oldest_in_group ( const group_t groupId,
                                      vsi_result*   result );

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_get_oldest_in_group ( lua_State* L )
{
    vsi_result*       results;
    vsi_signal_group  group;
    vsi_signal_group* temp = 0;

    //
    //  Go get the group ID from the caller's input parameters.
    //
    group.groupId = luaL_checkinteger ( L, 1 );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Go allocate a buffer that will hold all of the possible results from
    //  the "get newest" call that follows.
    //
    results = malloc ( sizeof(vsi_result) * temp->count );

    //
    //  Go get all of the newest signals for each signal defined in this
    //  group.
    //
    int status = vsi_get_oldest_in_group ( handle, temp->groupId, results );

    //
    //  Create a new table that will be our array of result tables.
    //
    lua_newtable ( L );

    //
    //  For each result object in this group...
    //
    for ( int i = 0; i < temp->count; ++i )
    {
        //
        //  Create a new result table.
        //
        lua_newtable ( L );

        //
        //  Populate all of the fields in this new result table.
        //
        lua_pushstring ( L, "domain" );
        lua_pushinteger ( L, results[i].domainId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "signal" );
        lua_pushinteger ( L, results[i].signalId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "name" );
        lua_pushstring ( L, results[i].name );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "data" );
        lua_pushinteger ( L, (unsigned long)results[i].data );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "dataLength" );
        lua_pushinteger ( L, results[i].dataLength );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "status" );
        lua_pushinteger ( L, results[i].status );
        lua_settable ( L, -3 );

        //
        //  Create a new entry in the array with this result table.
        //
        lua_settable ( L, -4 );
    }
    //
    //  Give back the memory containing the results.
    //
    free ( results );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    //
    //  Return 2 items to the caller.
    //
    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ g e t _ o l d e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the oldest signal for every member of the specified group.

    Lua Interface:

      status, results = Lua_vsi_get_oldest_in_group_wait ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.
            results - An array of results tables

        The "results" return value is an array of tables in which each table
        represents the results for one of the signals in the group and
        consists of:

            results.domainId   (integer)
            results.signalId   (integer)
            results.name       (string)
            results.data       (integer)
            results.dataLength (integer)
            results.status     (integer)

    The oldest member of each signal in the specified group will be retrieved
    and stored in the result array.  If any of the signals in the group do not
    contain any data, execution will be suspended for the current thread
    until some data is received for that signal.  When execution resumes, the
    next signal will be fetched and so forth until all of the signals have
    been retrieved.  At that point this function will return to the caller.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures must
    be at least as large as the number of signals defined in the specified
    group.  Failure to allocate enough space for the return data will result
    in memory corruption and undefined behavior (almost always very bad!).

    The signal data that is retrieved and returned to the caller will be
    removed from the VSI core database once it is copied into the result
    structures

    VSI API C Interface:

        int vsi_get_oldest_in_group_wait ( const group_t groupId,
                                           vsi_result*   result );

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_get_oldest_in_group_wait ( lua_State* L )
{
    vsi_result*       results;
    vsi_signal_group  group;
    vsi_signal_group* temp = 0;

    //
    //  Go get the group ID from the caller's input parameters.
    //
    group.groupId = luaL_checkinteger ( L, 1 );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Go allocate a buffer that will hold all of the possible results from
    //  the "get newest" call that follows.
    //
    results = malloc ( sizeof(vsi_result) * temp->count );

    //
    //  Go get all of the newest signals for each signal defined in this
    //  group.
    //
    int status = vsi_get_oldest_in_group_wait ( handle, temp->groupId, results );

    //
    //  Create a new table that will be our array of result tables.
    //
    lua_newtable ( L );

    //
    //  For each result object in this group...
    //
    for ( int i = 0; i < temp->count; ++i )
    {
        //
        //  Create a new result table.
        //
        lua_newtable ( L );

        //
        //  Populate all of the fields in this new result table.
        //
        lua_pushstring ( L, "domain" );
        lua_pushinteger ( L, results[i].domainId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "signal" );
        lua_pushinteger ( L, results[i].signalId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "name" );
        lua_pushstring ( L, results[i].name );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "data" );
        lua_pushinteger ( L, (unsigned long)results[i].data );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "dataLength" );
        lua_pushinteger ( L, results[i].dataLength );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "status" );
        lua_pushinteger ( L, results[i].status );
        lua_settable ( L, -3 );

        //
        //  Create a new entry in the array with this result table.
        //
        lua_settable ( L, -4 );
    }
    //
    //  Give back the memory containing the results.
    //
    free ( results );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    //
    //  Return 2 items to the caller.
    //
    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ l i s t e n _ a n y _ i n _ g r o u p

    @brief Listen for any signal in the specified group

    Lua Interface:

      status, domain, signal = Lua_vsi_listen_any_in_group ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.
            domain - An integer domain ID.
            signal - An integer signal ID.

    This function will listen for any signal in the specified group.  The
    signal list must have been previously built by adding the desired signals
    to the group before making this call.  The first signal received after
    this call is made will return it's value and the function will return to
    the caller.

    Unlike many of the other functions which return data for a group, this
    function does not expect (nor returns) an array of result structures.  It
    only returns a single result for the first signal in the group that
    contains data.

    VSI API C Interface:

        int vsi_listen_any_in_group ( const group_t groupId,
                                      unsigned int  timeout,
                                      domain_t*     domainId,
                                      signal_t*     signalId );

    @param[in] groupId - The address of where to store the group ID
    @param[in] timeout - The optional timeout value in nanoseconds
    @param[out] domainId - The address of where to store the domain ID
    @param[out] signalId - The address of where to store the signal ID

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
static int Lua_vsi_listen_any_in_group ( lua_State* L )
{
    domain_t domainId = 0;
    signal_t signalId = 0;
    int      status   = 0;

    unsigned int group = lua_tointeger ( L, 1 );

    status = vsi_listen_any_in_group ( handle, group, 0, &domainId, &signalId );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );
    lua_pushinteger ( L, domainId );
    lua_pushinteger ( L, signalId );

    return 3;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ l i s t e n _ a l l _ i n _ g r o u p

    @brief Listen for all signals in the specified group.

    Lua Interface:

      status, results = Lua_vsi_listen_all_in_group ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.
            results - An array of results tables

        The "results" return value is an array of tables in which each table
        represents the results for one of the signals in the group and
        consists of:

            results.domainId   (integer)
            results.signalId   (integer)
            results.name       (string)
            results.data       (integer)
            results.dataLength (integer)
            results.status     (integer)

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

    VSI API C Interface:

        int vsi_listen_all_in_group ( const group_t groupId,
                                      vsi_result*   results,
                                      unsigned int  resultsSize,
                                      unsigned int  timeout );

    @param[out] resultsPtr - The address of where to store the results
    @param[in] resultsSize - The size of the results array being supplied
    @param[in] timeout - The optional timeout value in nanoseconds

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
static int Lua_vsi_listen_all_in_group ( lua_State* L )
{
    int               status;
    vsi_result*       results;
    vsi_signal_group  group;
    vsi_signal_group* temp = 0;

    //
    //  Go get the group ID from the caller's input parameters.
    //
    group.groupId = luaL_checkinteger ( L, 1 );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Go allocate a buffer that will hold all of the possible results from
    //  the "get newest" call that follows.
    //
    results = malloc ( sizeof(vsi_result) * temp->count );

    //
    //  Go get all of the newest signals for each signal defined in this
    //  group.
    //
    status = vsi_listen_all_in_group ( handle, temp->groupId, results,
                                       temp->count, 0 );
    //
    //  Create a new table that will be our array of result tables.
    //
    lua_newtable ( L );

    //
    //  For each result object in this group...
    //
    for ( int i = 0; i < temp->count; ++i )
    {
        //
        //  Create a new result table.
        //
        lua_newtable ( L );

        //
        //  Populate all of the fields in this new result table.
        //
        lua_pushstring ( L, "domain" );
        lua_pushinteger ( L, results[i].domainId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "signal" );
        lua_pushinteger ( L, results[i].signalId );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "name" );
        lua_pushstring ( L, results[i].name );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "data" );
        lua_pushinteger ( L, (unsigned long)results[i].data );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "dataLength" );
        lua_pushinteger ( L, results[i].dataLength );
        lua_settable ( L, -3 );

        lua_pushstring ( L, "status" );
        lua_pushinteger ( L, results[i].status );
        lua_settable ( L, -3 );

        //
        //  Create a new entry in the array with this result table.
        //
        lua_settable ( L, -4 );
    }
    //
    //  Give back the memory containing the results.
    //
    free ( results );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );

    //
    //  Return 2 items to the caller.
    //
    return 2;
}


/*!-----------------------------------------------------------------------

    L u a _ v s i _ f l u s h _ g r o u p

    @brief Flushes all unread messages from the VSI message queue for each
           signal in the group.

    Lua Interface:

      status = Lua_vsi_flush_group ( group )

        Input arguments:
            group - An integer group ID.

        Return values:
            status - The integer completion status of the function.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the queues of every
    signal in the group.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    VSI API C Interface:

        int vsi_flush_group ( vsi_handle handle, const group_t groupId );

    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_flush_group ( lua_State* L )
{
    unsigned int group = luaL_checkinteger ( L, 1 );

    int status = vsi_flush_group ( handle, group );

    lua_pushinteger ( L, status );

    return 1;
}


/*!-----------------------------------------------------------------------

    N a m e / I D   M a n i u p l a t i o n   F u n c t i o n s
    ===========================================================

    L u a _ v s i _ n a m e _ s t r i n g _ t o _ i d

    @brief Convert a signal name to it's domain and signal ID values.

    Lua Interface:

      status, domain, signal = Lua_vsi_name_string_to_id ( name )

        Input arguments:
            name - The string name of the signal to translate.

        Return values:
            status - The integer completion status of the function.
            domain - The integer domain ID of the signal
            signal - The integer signal ID of the signal

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The domainId is a pointer to an identifier in which the signal domain will
    be stored.

    The signalId is a pointer to an identifier in which the signal ID will
    be stored.

    VSI API C Interface:

        int vsi_name_string_to_id ( const char* name,
                                    domain_t*   domainId,
                                    signal_t*   signalId );

    @param[in] - name - The name of the signal to be converted.
    @param[out] - domainId - A pointer in which the domain ID will be stored.
    @param[out] - signalId - A pointer in which the signal ID will be stored.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_name_string_to_id ( lua_State* L )
{
    domain_t domainId = 0;
    signal_t signalId = 0;
    int      status   = 0;

    const char* name = lua_tostring ( L, 1 );

    status = vsi_name_string_to_id ( handle, name, &domainId, &signalId );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );
    lua_pushinteger ( L, domainId );
    lua_pushinteger ( L, signalId );

    return 3;
}


/*!-----------------------------------------------------------------------

    v s i _ s i g n a l _ i d _ t o _ s t r i n g

    @brief Convert a signal domain and ID to it's ASCII string name.

    Lua Interface:

      status, name = Lua_vsi_name_string_to_id ( domain, signal )

        Input arguments:
            domain - The integer domain ID of the signal
            signal - The integer signal ID of the signal

        Return values:
            status - The integer completion status of the function.
            name - The string name of the signal to translate.

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

    VSI API C Interface:

		int vsi_signal_id_to_string ( const domain_t domainId,
									  const signal_t signalId,
									  char**         name );

    @param[in] - domainId - The signal domain ID to be converted.
    @param[in] - signalId - The signal ID to be converted.
    @param[in/out] - name - A pointer to a buffer in which to store the name.
    @param[in] - nameLength - The size of the name buffer being supplied.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
static int Lua_vsi_signal_id_to_string ( lua_State* L )
{
    unsigned int domainId = 0;
    unsigned int signalId = 0;
    char*        name     = 0;

    domainId = lua_tointeger ( L, 1 );
    signalId = lua_tointeger ( L, 2 );

    int status = vsi_signal_id_to_string ( handle, domainId, signalId, &name );

    //
    //  Return the status code to the caller.
    //
    lua_pushinteger ( L, status );
    lua_pushstring ( L, name );

    return 2;
}


/*!-----------------------------------------------------------------------

	L u a _ v s i _ d e f i n e _ s i g n a l _ n a m e

    @brief Define new signal Id/Name definition record.

    Lua Interface:

      status = Lua_vsi_define_signal_name ( domain, signal, privateId, name )

        Input arguments:
            domain - The integer domain ID of the signal
            signal - The integer signal ID of the signal
            privateId - The private integer ID of the signal
            name - The string name of the signal to translate.

        Return values:  None

    This function will define a new signal Id/Name definition record.  This
    record will be inserted into the signal Id and signal Name btree indices
    to allow conversions between the Id and Name of a signal and vice versa.

    VSI API C Interface:

		int vsi_define_signal_name ( const domain_t domainId,
									 const signal_t signalId,
									 const signal_t privateId,
									 const char*    name );

    @param[in] domainId - The signal domain ID to be defined.
    @param[in] signalId - The signal ID to be defined.
    @param[in] privateId - The private ID to be defined.
    @param[in] name - The ASCII name of the signal to be defined.

    @return None

------------------------------------------------------------------------*/
static int Lua_vsi_define_signal_name ( lua_State* L )
{
    unsigned int domainId  = lua_tointeger ( L, 1 );
    unsigned int signalId  = lua_tointeger ( L, 2 );
    unsigned int privateId = lua_tointeger ( L, 3 );
    const char*  name      = lua_tostring  ( L, 4 );

    vsi_define_signal_name ( handle, domainId, signalId, privateId, name );

    return 0;
}


/*!-----------------------------------------------------------------------

    l u a o p e n _ l i b l u a _ v s i _ a p i

	@brief Register the Lua functions we have implemented here.

	This function will make calls to Lua to register the names of all of the
    functions that have been implemented in this library.

------------------------------------------------------------------------*/
int luaopen_liblua_vsi_api ( lua_State* L )
{
    lua_register ( L, "Lua_vsi_initialize", Lua_vsi_initialize );
    lua_register ( L, "Lua_vsi_destroy", Lua_vsi_destroy );
    lua_register ( L, "Lua_vsi_VSS_import", Lua_vsi_VSS_import );
    lua_register ( L, "Lua_vsi_fire_signal", Lua_vsi_fire_signal );
    lua_register ( L, "Lua_vsi_get_oldest_signal", Lua_vsi_get_oldest_signal );
    lua_register ( L, "Lua_vsi_get_newest_signal", Lua_vsi_get_newest_signal );
    lua_register ( L, "Lua_vsi_flush_signal", Lua_vsi_flush_signal );
    lua_register ( L, "Lua_vsi_create_signal_group", Lua_vsi_create_signal_group );
    lua_register ( L, "Lua_vsi_delete_signal_group", Lua_vsi_delete_signal_group );
    lua_register ( L, "Lua_vsi_add_signal_to_group", Lua_vsi_add_signal_to_group );
    lua_register ( L, "Lua_vsi_remove_signal_from_group", Lua_vsi_remove_signal_from_group );
    lua_register ( L, "Lua_vsi_get_newest_in_group", Lua_vsi_get_newest_in_group );

    lua_register ( L, "Lua_vsi_get_newest_in_group_wait", Lua_vsi_get_newest_in_group_wait );
    lua_register ( L, "Lua_vsi_get_oldest_in_group", Lua_vsi_get_oldest_in_group );
    lua_register ( L, "Lua_vsi_get_oldest_in_group_wait", Lua_vsi_get_oldest_in_group_wait );
    lua_register ( L, "Lua_vsi_listen_any_in_group", Lua_vsi_listen_any_in_group );
    lua_register ( L, "Lua_vsi_listen_all_in_group", Lua_vsi_listen_all_in_group );
    lua_register ( L, "Lua_vsi_flush_group", Lua_vsi_flush_group );
    lua_register ( L, "Lua_vsi_name_string_to_id", Lua_vsi_name_string_to_id );
    lua_register ( L, "Lua_vsi_signal_id_to_string", Lua_vsi_signal_id_to_string );
    lua_register ( L, "Lua_vsi_define_signal_name", Lua_vsi_define_signal_name );

    return 0;
}


#endif  // End of #ifndef LUA_VSI_API_H

/*! @} */

// vim:filetype=h:syntax=c
