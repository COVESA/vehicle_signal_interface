# Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

default: api

CFLAGS+=-std=gnu11 -O4
LDFLAGS+=-lpthread -lc

TARGETS=api sample

all: $(TARGETS)

# Target for building the API library.
api: vsi.c $(INCLUDES)
	gcc $(CFLAGS) -shared -o libvsi.so -c vsi.c $(LDFLAGS) -fPIC

# Sample application that uses the API.
sample: sample.c $(INCLUDES)
	gcc $(CFLAGS) -o sample sample.c $(LDFLAGS) -Wl,-rpath ./ -L./ -lvsi

# Build the user-facing documentation.
docs: Doxyfile $(INCLUDES)
	doxygen ./Doxyfile

clean:
	rm -rf *.o *~ *.so html latex sample
