#
#   Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
#
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this file,
#   You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
#   Vehicle Signal Interface Core library build rules.
#

#
#   See if the user has set any of the environment variables...
#
#   The following environment variables are recognized:
#
#      VSI_DEBUG - Enable debugging mode.  Displays info messages during
#                  program execution.
#
#      VSI_TRACE - Enable function tracing mode.  Displays an indented call
#                  trace graph with Entry/Exit messages for all functions.
#                  This mode also indents VSI_DEBUG messages if they are
#                  enabled.
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

#
#   Define the C compiler flags that we want to use.
#
CFLAGS +=    \
  -std=gnu11 \
  -g         \
  -Wall      \
  -fPIC      \
  -pthread   \
  -I../core  \

#
#   Define the linker flags that we want to use.
#
LDFLAGS +=   \
  -L.        \
  -L../core  \
  -lpthread  \
  -lc        \

LDLIBS +=    \
  -lvsi      \
  -lvsi-core \

#
#   Define the list of include files we use.
#
INCLUDES=    \
  vsi.h      \
  vsi_list.h \
  btree.h    \

#
#   Define the list of targets to be built.
#
TARGETS =   \
  libvsi.so \
  sample    \
  importVSS \

#
#   Define any extra files to be included in a tar output file.
#
EXTRA_FILES=  \
  Makefile    \
  README.md   \

#
#   Allow make to run compiles/links in parallel.
#
MAKEFLAGS += j

#
#   Suppress all of the automatic command echoing.
#
.SILENT:

default: all

all: $(TARGETS)

# Target for building the API library.
api libvsi.so: vsi.c vsi_list.c $(INCLUDES)
	echo "Rebuilding the VSI library..."; \
	gcc $(CFLAGS) $(DEFINES) -shared -o libvsi.so \
        vsi.c vsi_list.c btree.c $(LDFLAGS) -lvsi-core

importVSS: importVSS.o libvsi.so
	echo "Rebuilding the VSS import executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o $@ importVSS.o \
        $(LDFLAGS) -Wl,-rpath $(CURDIR) -Wl,-rpath $(CURDIR)/../core \
		$(LDLIBS)

# Sample application that uses the API.
sample: sample.o $(INCLUDES) libvsi.so
	echo "Rebuilding the sample executable..."; \
	gcc $(CFLAGS) $(DEFINES) -o sample sample.o \
        $(LDFLAGS) -Wl,-rpath $(CURDIR) -Wl,-rpath $(CURDIR)/../core \
		$(LDLIBS)

# Build the user-facing documentation.
docs: Doxyfile $(INCLUDES)
	echo "Rebuilding the VSI documentation..."; \
	doxygen ./Doxyfile

rebuild:
	echo "Rebuilding all the executables and libraries..."; \
	make -B $(TARGETS)

clean distclean:
	echo "Deleting all derived files..."; \
	rm -rf *.o *~ libvsi.so html latex sample importVSS core
