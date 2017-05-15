cmake_minimum_required (VERSION 3.0.0)

project(vehicle_signal_interface C)

option(BUILD_TESTS "Build test programs" ON)
option(BUILD_LUA "Build Lua interface" ON)
option(BUILD_PYTHON "Build Python interface" ON)

add_definitions('-Wall')
include_directories(${CMAKE_SOURCE_DIR}/api)
include_directories(${CMAKE_SOURCE_DIR}/core)

add_subdirectory(api)
add_subdirectory(core)

if(BUILD_TESTS)
    add_subdirectory(api/tests)
    add_subdirectory(core/tests)
endif()

if(BUILD_LUA)
    add_subdirectory(lua-interfaces)
endif()

if(BUILD_PYTHON)
    # Use version 3 by default. Can be overriden with -DMIN_PYTHON_VERSION=2.7 commandline option
    set(MIN_PYTHON_VERSION "${MIN_PYTHON_VERSION}" CACHE STRING "3")

    add_subdirectory(python-interfaces)
endif()

