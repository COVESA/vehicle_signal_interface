if(${CMAKE_VERSION} VERSION_GREATER 3)
    find_package( Lua 5.1 REQUIRED )
else()
    find_package( Lua51 5.1 REQUIRED )
endif()

set(SRC
    lua_vsi.c
    lua_vsi_api.c
    lua_vsi_core.c
)

include_directories(${LUA_INCLUDE_DIR})

add_library(lua_vsi SHARED ${SRC})
