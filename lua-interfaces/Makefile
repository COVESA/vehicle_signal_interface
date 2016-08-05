#
#   Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
#
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this file,
#   You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
#	Vehicle Signal Interface Core library build rules.
#

#
#	See if the user has set any of the environment variables...
#
#	The following environment variables are recognized:
#
#      VSI_DEBUG - Enable debugging mode.  Displays info messages during
#                  program execution.
#
#      VSI_TRACE - Enable function tracing mode.  Displays an indented call
#                  trace graph with Entry/Exit messages for all functions.
#                  This mode also indents VSI_DEBUG messages if they are
#                  enabled.
#
#      TRACE_PRINT - Enable function tracing debug mode.  This is normally only
#                    used to debug problems in the tracing mode functions.
#
DEFINES =

ifdef VSI_DEBUG
  $(info Setting VSI_DEBUG mode...)
  DEFINES += -DVSI_DEBUG
endif

ifdef VSI_TRACE
  $(info Setting VSI_TRACE mode...)
  DEFINES += -DVSI_TRACE -finstrument-functions -Wl,--export-dynamic
  LDFLAGS += trace.o -ldl
  INCLUDES += trace.h
endif

ifdef TRACE_PRINT
  $(info Setting TRACE_PRINT mode...)
  DEFINES += -DTRACE_PRINT
endif

#
#	Define the C compiler flags that we want to use.
#
CFLAGS+=     \
  -I.        \
  -I../core  \
  -I../api   \
  -std=gnu11 \
  -g         \
  -Wall      \
  -fPIC      \
  -pthread   \

#
#	Define the linker flags that we want to use.
#
LDFLAGS+=    \
  -L.        \
  -L../core  \
  -lpthread  \
  -lc		 \

LDLIBS += \
  -lvsi-core \

#
#	Define the list of include files we use.
#
INCLUDES=             \

#
#	Define the list of targets to be built.
#
TARGETS=         \
  liblua_vsi_core.so \

#
#	Define any extra files to be included in a tar output file.
#
EXTRA_FILES=  \
  Makefile    \
  README.md   \

#
#	Allow make to run compiles/links in parallel.
#
MAKEFLAGS += j

#
#	Suppress all of the automatic command echoing.
#
.SILENT:

#
#	Build everything by default.
#
default: $(TARGETS)

#
#	Define the rule that will unconditionally rebuild all of the targets.
#
rebuild:
	make -B $(TARGETS)

#
#	Define the rule to build the shared library file.
#
liblua_vsi_core.so: lua_vsi_core.o
	echo "Linking the $@ shared library..."; \
	gcc $(CFLAGS) $(DEFINES) -shared -o $@ lua_vsi_core.o -lvsi-core $(LDFLAGS)

#
#	Define the rule to build the tar package that will contain all of the
#	source files, extra files, and executables.
#
tar:
	echo "Creating the development tar package..."; \
	make rebuild; \
	tar -cvzf vsiCore.tz *.c *.h $(EXTRA_FILES) $(TARGETS)

#
#	Define the rules to remove the local generated files.
#
clean:
	echo "Deleting all derived files..."; \
	rm -f *.o *~ $(TARGETS) vsiCore.tz

#
#	Define the rule to remove the shared memory segment file.  Removing it's
#	disk file is the only way to start with a fresh copy of the shared memory
#	segment.  Afer the file is removed, it will be automatically recreated the
#	next time someone attempts to open it.  This can also be done by executing
#	the command in the rule manaully if you prefer.
#
cleanshm:
	echo "Deleting the shared memory segment file.."; \
	rm -f /var/run/shm/vsiCoreDataStore

#
#	Define the rule that will remove all of the locally generated files and
#	remove the shared memory segment file.
#
distclean:
	make -s clean; \
	make -s cleanshm

#
#	Define the rule to compile a C source file.
#
.c.o: $(INCLUDES)
	echo "Compiling $?";  \
	gcc $(CFLAGS) $(DEFINES) -c $?

