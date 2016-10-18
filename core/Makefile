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
DEFINES =

ifdef VSI_DEBUG
  $(info Setting VSI_DEBUG mode...)
  DEFINES += -DVSI_DEBUG
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
  -lpthread  \
  -lc		 \

LDLIBS += \
  -lvsi-core \

#
#	Define the list of include files we use.
#
INCLUDES=             \
  sharedMemory.h      \
  sharedMemoryLocks.h \
  vsi_core_api.h      \
  utils.h             \

#
#	Define the list of targets to be built.
#
TARGETS=         \
  libvsi-core.so \
  dump           \
  insert         \
  fetch          \
  flush          \
  writeRecord    \
  readRecord     \

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
libvsi-core.so: vsi_core_api.o sharedMemory.o sharedMemoryLocks.o utils.o
	echo "Linking the $@ shared library..."; \
	gcc $(CFLAGS) $(DEFINES) -shared -o $@ vsi_core_api.o sharedMemory.o \
		sharedMemoryLocks.o utils.o $(LDFLAGS)

#
#	Define the rules to build all of the utility executables.
#
insert : insert.o libvsi-core.so $(INCLUDES)
	echo "Linking the $@ executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

fetch : fetch.o libvsi-core.so $(INCLUDES)
	echo "Linking the $@ executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

flush : flush.o libvsi-core.so $(INCLUDES)
	echo "Linking the $@ executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

dump : dump.o libvsi-core.so $(INCLUDES)
	echo "Linking the $@ executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

writeRecord : writeRecord.o libvsi-core.so $(INCLUDES)
	echo "Linking the $@ executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

readRecord : readRecord.o libvsi-core.so $(INCLUDES)
	echo "Linking the $@ executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ $@.o $(LDFLAGS) $(LDLIBS)

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

