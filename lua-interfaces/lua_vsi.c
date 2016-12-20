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

#ifndef LUA_VSI_H
#define LUA_VSI_H


#include <lua.h>
#include <lauxlib.h>


/*! @{ */


/*!-----------------------------------------------------------------------

    l u a o p e n _ l i b l u a _ v s i

	@brief Register the Lua functions we have implemented here.

	This function will make calls to Lua to register the names of all of the
    functions that have been implemented in this library.

------------------------------------------------------------------------*/
extern int luaopen_liblua_vsi_core ( lua_State* L );
extern int luaopen_liblua_vsi_api ( lua_State* L );

int luaopen_liblua_vsi ( lua_State* L )
{
    luaopen_liblua_vsi_core ( L );
    luaopen_liblua_vsi_api ( L );
    return 0;
}


#endif  // End of #ifndef LUA_VSI_H

/*! @} */

// vim:filetype=h:syntax=c
