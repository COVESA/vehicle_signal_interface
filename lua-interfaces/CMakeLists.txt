find_package( Lua 5.1 REQUIRED )

set(SRC
    lua_vsi.c
    lua_vsi_api.c
    lua_vsi_core.c
)

include_directories(${LUA_INCLUDE_DIR})

add_library(lua_vsi SHARED ${SRC})
