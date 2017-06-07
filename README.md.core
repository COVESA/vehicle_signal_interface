**(C) 2016 Jaguar Land Rover - All rights reserved.**

All documents in this repository are licensed under the Creative Commons
Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.  All code in
this repository is licensed under Mozilla Public License v2 (MPLv2). Click
[here](https://www.mozilla.org/en-US/MPL/2.0/) for details.

## VEHICLE SIGNAL INTERFACE - Core Library Code
This directory contains the second iteration of the core library code.  The
first iteration was merely a proof of concept and some preliminary performance
figures.

The files here are somewhat static at the moment and not being worked on any
more.  Once proof of concept was achieved, work switched to the phase 3 (and
hopefully final) of the code which will implement a robust shared memory
memory manager with dynamic data structures.  Because of that, documentation
is mostly limited to the contents of the source files and much of the code is
unfinished and may contain "debug" code.

This version of the core has a statically defined shared memory segment but is
quite usable for many projects.

The default configuration for it is a shared memory area of approximately 1GB
in size.  The size and location of the image file of the shared memory segment
can be easily customized in the sharedMemory.h file (look for the "#define"
statements - The size and location of the file is defined part way through the
file).

There is considerable documentation in the header files concerning exactly how
each of the components of the library operate.

The library file (libvsi-core.so) is made up of the following:

```C
    sharedMemory.h
    sharedMemory.c
    sharedMemoryLocks.h
    sharedMemoryLocks.c
    utils.h
    utils.c
    vsi-core-api.h
    vsi-core-api.c
```

There are some testing functions defined in:

```C
    fetch.c
    insert.c
    dump.c
```

There are some utility functions that can be used for manual testing or
rudimentary operations on the data store:

```C
    readRecord.c
    writeRecord.c
```

All of the libraries and executable can be created by typing "make" in the
directory.  The shared memory image file can be removed with "make cleanshm",
and everything can be removed for a fresh start with "make distclean".  See
the Makefile for other possible targets.

### VSI User API

The user API for the VSI core code is relatively small.  Details of the
implementations for each function can be found in the source and header files
in this directory.  The following is a short description of each function and
what it does.

The domain parameter in the following is not currently implemented and this
parameter will be ignored by all of the API functions.

The body parameters are expected to be pointers into a user's data buffer
somewhere in his virtual address space.  This is not an offset into the shared
memory segment, just a normal C data pointer.

All key values in this implementation are required to be unique.  If a
duplicate key is specified in the insert function, an error will be returned
and the record will not be inserted.

#### vsi_core_open

```C
    vsi_core_handle vsi_core_open ( bool createNew );
```

This call is mandatory before any other function can be executed on the data
in the shared memory segment.  The code will open an existing instance of the
shared memory segment or if none exists or one exists but contains no data, a
new shared memory segment will be automatically created and initialized.

If the "createNew" argument is "true" then a new empty data store will be
created and opened.

The return value is a handle to the shared memory segment and will be needed
for subsequent calls to the API functions.

#### vsi_core_insert

This function will insert a new message into the shared memory segment at the proper
location with the specified key and domain, and body values.

```C
    int vsi_core_insert ( vsi_core_handle handle,
                          offset_t        key,
                          enum domains    domain,
                          unsigned long   newMessageSize,
                          void*           body );
```

The handle parameter is a pointer to the shared memory segment that was
returned from the sharedMemoryOpen call.

The key is an unsigned 8 byte value which will be used to retrieve this data
record.  If the key is not unique, the record will stored in the shared memory
segment in the order in which they were inserted and fetching them will always
retrieve the oldest record available.  Note that records are not stored
indefinitely.  If the maximum amount of data for a given key exceeds the
maximum byte count, the oldest records will be overwritten by subsequent
inserts and those overwritten records will be lost and cannot be retrieved.

The domain is an identifier for which domain this record should be stored in.
Domains are not currently implemented so this value is ignored.

The bodySize is the length of the body data that is to be stored in the shared
memory segment.

The body is a pointer to the memory location containing the body data that is
to be store in the shared memory segment.

#### vsi_core_fetch

This function will find the record with the specified domain and key values in
the shared memory segment, return the message data to the caller and delete
the message from the shared memory segment.

```C
    int vsi_core_fetch ( vsi_core_handle handle,
                         unsigned long   key,
                         enum domains    domain,
                         unsigned long   bodySize,
                         void*           body );
```

The handle parameter is a pointer to the shared memory segment that was
returned from the sharedMemoryOpen call.

The key is an unsigned 8 byte value which will identify the data record in the
shared memory segment the user wishes to retrive.  If the specified key is not
found, an error will be indicated with the return value.

The domain is an identifier for which domain this record should be stored in.
Domains are not currently implemented so this value is ignored.

The body size parameter is the size of the user's data buffer being supplied.
This is the maximum number of bytes of data that will be copied into the
supplied buffer by this code.  This value should be set to the maximum size of
the data buffer being supplied.  If the data buffer supplied is not large
enough to contain the entire data record, the data will be truncated to fix
into the buffer.

The body pointer is the address of the buffer in which the record data will be
copied.  Note that the data is copied as binary data and will not
automatically have a null appended to it if it is actually ASCII data.  It is
the caller's resonsibility to ensure that if the data being retrieved is ASCII
data, that those strings are properly null terminated.

#### vsi_core_close

```C
    void vsi_core_close ( vsi_core_handle handle );
```

The handle parameter is a pointer to the shared memory segment that was
returned from the sharedMemoryOpen call.

This call will unmap the current shared memory segment from the user's virtual
address space and ensure that the data has been saved in the disk file
associated with this shared memory segment.

This call is not strictly required as the operating system (at least on Linux)
will automatically save and close the shared memory segment when the process
terminated.  However, it is recommended that the shared memory segment be
closed as there may be cleanup work performed upon close.

### VSI Utilities

The following executables are automatically built by the Makefile in the
source directory.  All of the following utilites have built-in help
functionality that can be gotten with the "-h" or "-?" flags.

#### dump

This is a utility that will dump the contents of the shared memory segment in
a human readable format.

Example [dump the first message of the first hash bucket]:

```C
    % dump -m 1 -b 1
    Shared memory segment has been mapped into memory
    Beginning dump of shared memory segment[/var/run/shm/vsiSharedMemorySegment]...
    Hash Bucket 0[0]:
    Head offset............: 0
    Tail offset............: 39,040
    Message count..........: 977
    Generation number......: 0
    Message sequence number: 977
    Total message size.....: 1,048,576
    Hash bucket size.......: 1,048,576
       Message number 1[0x7ff779b3b0b8-0]:
          Key.................: 0
          Domain..............: 1
          Message size........: 8
          Next message offset.: 40
          Data (8 bytes @ 0x7ff779b3b0d4):
          000000  00 00 00 00 40 42 0f 00                         ....@B..
```

#### insert

This utility can be used to benchmark the speed of record insertion.  It will
insert some number of records and then print out statistics about the
performance of that operation.  The default records count in 1 million but
that can be modified via command line arguments.

Example:

```C
    % insert
    Shared memory segment has been mapped into memory
    1,000,000 records in 218,883,330 nsec. 218 msec. - 4,568,643 records/sec - Avg: 4,568,643
```


#### fetch

This utility can be used to benchmark the speed of record retrieval.  It will
retrieve some number of records and then print out statistics about the
performance of that operation.  The default records count in 1 million but
that can be modified via command line arguments.

Example:

```C
    % fetch
    Shared memory segment has been mapped into memory
    1,000,000 records in 187,857,828 nsec. 187 msec. - 5,323,174 records/sec - Avg: 5,323,174
```

#### writeRecord

This utility can be used to write a single record into the shared memory
segment.  If another process was waiting for the record that was written, it
will be allowed to continue.

Example:

```C
    % writeRecord -k 23 -a 23abc
    Using key value[23]
    ASCII body data[23abc] will be used.
    Shared memory segment has been mapped into memory
```

#### readRecord

This utility can be used to read a single record from the shared memory
segment given the key for the record.  If no record with the spcified key is
available, it will wait for one to be written into the shared memory segment.

Example:

```C
    % readRecord -k 23
    Shared memory segment has been mapped into memory
      domain: 1
      key...: 23
      value.: 23[23abc]
```

Note that in the above example, the value is displayed in 2 formats.  The
first is a numeric interpretation of the data in the buffer (as though the
buffer contents had been processed with "atol"), and the second is the ASCII
representation of the data bytes in the buffer.  One or the other of these
will most likely be incorrect depending on how the data was inserted.

#### flush

This utility can be used to delete all of the records with the specified
domain and key from the data store.

Example:

```C
    % flush -k 23
    Shared memory segment has been mapped into memory
      domain: 1
      key...: 23
      return: 0
```

### Caveats

The current implementation of this VSI data store uses statically defined data
structures.  These have been defined in the code as fairly substantial with
1024 hash buckets and each hash bucket having space for 1MB of signal data for
a total of about 1GB of storage space.  Obviously not all of that space will
be in use (escpecially during development) and it is just a small chunk of the
available virtual address space in a 64-bit environment but if this is not
acceptable for your specific environment, feel free to adjust the parameters
in any way you wish.  They are all defined in the sharedMemory.h file.

Because this VSI implementation is built with a hash table of one-way linked
lists, it is possible for more than one signal key to wind up hashing to the
same hash bucket.  This results in multiple keys existing intermixed with each
other in each hash bucket list.  Since the lists are therefore not homogeneous
with respect to the key values, some of the logic is more complicated than it
would be otherwise.  There are times when the lists must be sequentially
accessed to perform the requested operation which will take more time than is
optimal.  Future implementations of the VSI shared memory data structures will
address this issue and allow dynamically defined data structures (e.g. "map"
type structures for instance).

In the current implementation, the domain values that are passed into most of
the functions are not used as part of the signal hash.  This will also be
fixed in later revisions of the code.

### Building

Building this code is very straight forward.  Merely copy all of the source
file into a directory and assuming you have a Linux development environment
(i.e. with gcc, make, ar, etc.) all you need to do is type "make".  By
default, everything will be compiled and all of the executables linked.  At
that point, you are ready to go.

There are multiple make recipes which are useful and these can be easily
seen in the "Makefile".
