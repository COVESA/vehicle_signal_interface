# Do not print error in case FindLua module do not exist
find_package(Lua 5.1 QUIET)
if (NOT LUA_FOUND)
    find_package(Lua51 REQUIRED)
else()
    # If found do it again just to have information that Lua has been found
    find_package(Lua 5.1 REQUIRED)
endif()

set(SRC
    lua_vsi.c
    lua_vsi_api.c
    lua_vsi_core.c
)

include_directories(${LUA_INCLUDE_DIR})

add_library(lua_vsi SHARED ${SRC})
install(TARGETS lua_vsi LIBRARY DESTINATION lib)

