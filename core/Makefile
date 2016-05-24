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

CFLAGS+=     \
  -std=gnu11 \
  -g         \
  -Wall      \
  -fPIC      \

LDFLAGS+=    \
  -L.        \
  -lvsi-core \
  -lpthread  \
  -lc		 \

INCLUDES=             \
  sharedMemory.h      \
  sharedMemoryLocks.h \
  vsi_core_api.h      \
  utils.h             \

TARGETS=         \
  libvsi-core.so \
  dump           \
  insert         \
  fetch          \
  writeRecord    \
  readRecord     \

EXTRA_FILES=  \
  Makefile    \
  README.md   \

#
#	Suppress all of the automatic command echoing.
#
.SILENT:

default: all

all:  $(TARGETS)

rebuild:
	make -s distclean; \
	make all

insert : insert.o libvsi_core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

fetch : fetch.o libvsi_core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

dump : dump.o libvsi_core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

writeRecord : writeRecord.o libvsi_core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

readRecord : readRecord.o libvsi_core.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

libvsi-core.so: vsi_core_api.o sharedMemory.o sharedMemoryLocks.o utils.o
	echo "Linking the $@ shared library..."; \
	gcc $(CFLAGS) -shared -o $@ vsi_core_api.o sharedMemory.o sharedMemoryLocks.o utils.o

tar:
	echo "Creating the development tar package..."; \
	make clean; \
	make all; \
	tar -cvzf sviPrototype.tz *.c *.h $(EXTRA_FILES) $(TARGETS)

clean:
	echo "Deleting all derived files..."; \
	rm -f *.o *~ $(TARGETS) sviPrototype.tz

cleanshm:
	echo "Deleting the shared memory segment file.."; \
	rm -f /var/run/shm/vsiSharedMemorySegment

distclean:
	make -s clean; \
	make -s cleanshm


.c.o: $(INCLUDES)
	echo "Compiling $?";  \
	gcc $(CFLAGS) -c $?
