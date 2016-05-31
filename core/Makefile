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
#	Define the C compiler flags that we want to use.
#
CFLAGS+=     \
  -std=gnu11 \
  -g         \
  -Wall      \
  -fPIC      \

#
#	Define the linker flags that we want to use.
#
LDFLAGS+=    \
  -L.        \
  -lvsi-core \
  -lpthread  \
  -lc		 \

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
default: all

#
#	Define "all" as all of the targets we defined above.
#
all:  $(TARGETS)

#
#	Define the rule that will unconditionally rebuild all of the targets.
#
rebuild:
	make -B all

#
#	Define the rule to build the shared library file.
#
libvsi-core.so: vsi_core_api.o sharedMemory.o sharedMemoryLocks.o utils.o
	echo "Linking the $@ shared library..."; \
	gcc $(CFLAGS) -shared -o $@ vsi_core_api.o sharedMemory.o sharedMemoryLocks.o utils.o

#
#	Define the rules to build all of the utility executables.
#
insert : insert.o libvsi-core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

fetch : fetch.o libvsi-core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

flush : flush.o libvsi-core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

dump : dump.o libvsi-core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

writeRecord : writeRecord.o libvsi-core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

readRecord : readRecord.o libvsi-core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

#
#	Define the rule to build the tar package that will contain all of the
#	source files, extra files, and executables.
#
tar:
	echo "Creating the development tar package..."; \
	make rebuild; \
	tar -cvzf vsiCore.tz *.c *.h $(EXTRA_FILES) $(TARGETS)

#
#	Define the rules to remove generated files.
#
clean:
	echo "Deleting all derived files..."; \
	rm -f *.o *~ $(TARGETS) sviPrototype.tz

cleanshm:
	echo "Deleting the shared memory segment file.."; \
	rm -f /var/run/shm/vsiCoreDataStore

distclean:
	make -s clean; \
	make -s cleanshm

#
#	Define the rule t compile a C source file.
#
.c.o: $(INCLUDES)
	echo "Compiling $?";  \
	gcc $(CFLAGS) -c $?

