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
```
git clone https://github.com/GENIVI/vehicle_signal_interface.git
cd vehicle_signal_interface
mkdir build
cd build
cmake ..
make
```
### Building options
* BUILD_LUA - Build Lua interface. Default OFF
* BUILD_PYTHON - Build Python interface. Default ON
* MIN_PYTHON_VERSION - Define minimal Python version to be used. Default 3.

E.g. to enable LUA building and use Python 2.7 version:
```
cmake .. -DBUILD_LUA=ON -DMIN_PYTHON_VERSION=2.7
```
__NOTE:__ It may be required to completely clear build directory (i.e. rm -rf \*) to change building options

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
