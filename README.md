**(C) 2016 Jaguar Land Rover - All rights reserved.**

All documents in this repository are licensed under the Creative
Commons Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.
All code in this repository is licensed under Mozilla Public License
v2 (MPLv2). Click [here](https://www.mozilla.org/en-US/MPL/2.0/) for
details.

## Vehicle Signal Interface
This is the first revision of th VSI source code that contains the shared
memory data repository to allow dynamically built data structures to be
created in the VSI environment.

This allows very flexible high speed communication of data between threads or
tasks on a single processor node system.

This repository contains the library required to distribute vehicle signals
between components inside the IVI.

At this time, the API is rudimentary with extensions welcome by interested
users.

This version of the library creates 2 shared memory segments.  One is used
internally by the shared memory memory management system and the other is
available for user defined data.  By default, the initial sizes of these
segments are defined in the sharedMemory.h header file as defined constants
and can be modified by the user to his application.  This version of the code
defines a 1MB system segment and a 2MB user segment by default.

## Building

* Clone or download the entire directory tree from this location.
* Change directories to the "vehicle_signal_interface" directory.
* Build everything by typing "make" in this directory.
    * Dynamic shared libraries will be built in the core and api subdirectories.
    * The api directory contains a "sample" executable.
    * The core directory contains some executables for performance testing and
      scripted write/read of individual data records.
    * There is a python-interfaces directory that contains some sample Python
      interfaces for the VSI api functions.
    * There is a lua-interfaces directory that contains some sample Lua
      interfaces for the VSI api functions.  This directory is very sparse
      currently and may get fleshed out more if there is any interest in the
      Lua interfaces.
    * See the README files in the subdirectories for more information.
* At the top level, there are some directories that will be populated when the
  system is built.
    * The lib directory is the primary directory that will contain all of the
      built shared libraries with the API interfaces in them.  Putting this
      directory in you library search path is recommended.
    * The include directory will contain the header files that define the user
      API functions.
    * There are several other directories that are currently empty but will
      most likely be populated as this library matures.

## Running the Sample Executables

To run the "sample" program, change directories to the
"vehicle_signal_interface/api" directory and type "./sample".

Similarly, going to the "vehicle_signal_interface/core/tests" directory will allow
you to run the executables that were created in that directory.

## Caveats

All of this code is still under constant development so things are likely to
change in subsequent updates.

Some of the functions in the API have not yet been implemented and are stubbed
off for the time being.  Calling these functions right now will often result
in some dummy data being returned or a warning message that the function has
not yet been implemented.

As always, any and all feedback is welcome.

## Known Issues

There has been a request to add "extern C" to the header files to allow them
to be used with C++ builds.

The comparison function needs to be enhanced to define a more flexible
comparison type.  The immediate need is for a "string" type to allow string
keys.
