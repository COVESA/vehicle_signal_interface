cmake_minimum_required (VERSION 2.8)

project(vehicle_signal_interface C)

option(BUILD_TESTS "Build test programs" ON)
option(BUILD_LUA "Build Lua interface" ON)
option(BUILD_PYTHON "Build Python interface" ON)

include_directories(${CMAKE_SOURCE_DIR}/api)
include_directories(${CMAKE_SOURCE_DIR}/core)

find_package(Threads REQUIRED)
add_definitions(-Wall -std=c99 -D_GNU_SOURCE)

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

