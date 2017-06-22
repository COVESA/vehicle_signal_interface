cmake_minimum_required (VERSION 2.8)

project(vehicle_signal_interface C)

option(BUILD_LUA "Build Lua interface" ON)
option(BUILD_PYTHON "Build Python interface" ON)
option(BUILD_CAN "Build SocketCAN bindings" ON)

include_directories(${CMAKE_SOURCE_DIR}/src)

find_package(Threads REQUIRED)
add_definitions(-Wall -std=c99 -D_GNU_SOURCE)

add_subdirectory(src)

if(BUILD_LUA)
    add_subdirectory(lua-interfaces)
endif()

if(BUILD_PYTHON)
    # Use version 3 by default. Can be overriden with -DMIN_PYTHON_VERSION=2.7 commandline option
    set(MIN_PYTHON_VERSION "${MIN_PYTHON_VERSION}" CACHE STRING "3")

    add_subdirectory(python-interfaces)
endif()

if(BUILD_CAN)
    add_subdirectory(socketcan-bindings)
endif()

