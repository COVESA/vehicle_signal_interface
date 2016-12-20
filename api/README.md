**(C) 2016 Jaguar Land Rover - All rights reserved.**<br>

All documents in this repository are licensed under the Creative
Commons Attribution 4.0 International (CC BY 4.0). Click
[here](https://creativecommons.org/licenses/by/4.0/) for details.
All code in this repository is licensed under Mozilla Public License
v2 (MPLv2). Click [here](https://www.mozilla.org/en-US/MPL/2.0/) for
details.

## VEHICLE SIGNAL INTERFACE - API
This repository contains the API required to interact with the Vehicle Signal
Interface, a software bus designed for rapid signal propagation.

All signals to be used with this API are specified by the Vehicle Signal
Specification, located
**[here](https://github.com/PDXostc/vehicle_signal_specification)**.

### Basic Signal Interactions
Users of the API have two basic mechanisms available to them for operating on
signals.  They can operate on individual signals or they can operate on groups
of signals.  The functions available for each class will be detailed below.

Signals have both an ID, which is numeric, and a name, which is an ASCII
string.  There are 2 versions of most of the functions.  One which takes
signal names and one that takes signal IDs.  The two are interchangeable and
operate identically and should always return identical information.  The
"by name" functions will convert the supplied name into it's corresponding ID
and then call the "by ID" function that corresponds to the same operation.

The VSI API provides functions to convert between a named signal and its ID
and vice versa. It will only use the ID internally, so applications interested
in maximum performance are encouraged to use the signal IDs directly.

### Basic Data Structures
The following are the definitions of the basic VSI data structures the user
needs to be familiar with.  These structures are used in some of the API
functions defined below and may require the callers to create and supply some
of them for those functions.

#### Simple Structure Types
There are number of simple data types defined such as "domain_t" and
"signal_t" that are simple typedefs and are defined in the "vsi.h" header
file.  See that file if the definitions of these types is required, they will
not be defined here again.

#### vsi_result

```C
	typedef struct vsi_result
	{
		pthread_mutex_t lock;

		vsi_context*    context;

		domain_t        domainId;
		signal_t        signalId;
		char*           name;

		char*           data;
		unsigned long   dataLength;

		int             status;

	}   vsi_result;
```

Many of the functions in the VSI API take a ```vsi_result``` structure as an
argument to the function and return the results of the function in that same
structure.

The contents of this structure must be initialized with the information
required by the function being called as input (often the domain and signal ID
or the signal name) and the results of the function execution will be returned
in the structure upon exit.

Note that when the data and dataLength fields are required, the caller is
responsible for allocating a memory area large enough to contain the data
being asked for.  The dataLength parameter then specifies the size of that
allocated memory area.  Many of the functions that return data in these fields
will replace the dataLength value with the actual number of bytes written into
the data buffer before returning to the caller.  If the returned data length
is the same as the input data length, the supplied buffer was not large enough
to hold all of the data and has therefore been truncated.  The caller should
make the call again with a larger buffer if he wants all of the data
available.

Return data will always be written with the minimum of either the size of the
data being returned or the size of the supplied buffer, whichever one is
smallest.  No data in the buffers past the actual returned data will be
changed, that is, the buffer will not be padded with any data to fill it out
to it's specified size if the amount of data actually being returned is
smaller than the buffer size.

The domainId is an identifier for the domain this record should be stored in.
This is currently defined as a small positive integer value.

The signalId is a numeric value which will identify the specific signal
desired by the caller within the domain specified.

The signal name is an ASCII string that uniquely identifies the signal in
question.  This name is often an alternative way to address a specific signal
being used in place of specifying the domainId and signalId fields.  Using
signal names provides a more platform independent way to access signal data
and should be used whenever possible by the higher level vehicle signal
handling modules.

The data pointer is the address of the buffer which constains the record data.
Note that the data is not assumed to be ASCII in nature so it is the caller's
responsibility to ensure that it is null terminated if need be.

The dataLength parameter is the size of the user's data buffer being supplied.
This is the maximum number of bytes of data that will be copied into the
supplied buffer by the API code.  This value should be set to the maximum size
of the data buffer being supplied.  If the data buffer supplied is not large
enough to contain the entire data record, the data will be truncated to fix
into the buffer.

Generally speaking, the "result" pointer defined in the functions that follow
is actually a pointer to an array of "vsi_result" structures.  If the operation
only operates on a single signal then this is an array of length 1 or simply a
single structure.  If the function being used can return data for multiple
signals then this pointer is actually a pointer to an array of "vsi_result"
structures where the number of structures must be at least as large as the
number maximum number of signals that could be returned by the call.

The caller is responsible for allocating and deallocating all of these
structures and buffers.

### General Operations
There are a number of operations on the VSI database that are not specific to
any signal or groups of signals.

#### vsi_initialize

```C
    vsi_handle vsi_initialize ( bool createNew );
```

This function will cause the VSI database to be loaded into memory and readied
for operation.  The function returns a VSI handle object that will need to be
supplied to the other functions to define the context for this specific user.

If the "createNew" parameter is "true", a brand new empty data store instance
will be created instead of opening an existing instance (assuming one exists).

This function should only be called once within each process that will access
the VSI data.

#### vsi_destroy

```C
    int vsi_destroy ( vsi_handle handle );
```

This function will terminate the VSI database instance that is specified by
the handle parameter.  Once this function is called, the VSI database is no
longer available for any access and attempting to access it after it has been
destroyed will result in unspecified behavior and very likely a SEGV
exception.

### Individual Signals
If a user wishes to operate on individual signals there are a variety of
functions available to do that.

Many of these functions have a "wait" parameter which can be specified to
govern whether or not the caller wants the call to hang waiting for a signal
to be received (if here is not one in the database already) or return
immediately.

If the user specifies "no wait" and there is no signal available of the type
the user specified, the function will return an error code indicating that
there is no data available.

If the user specifies "wait" and there is no signal available of the type the
user specified, the function will hang on an internal semaphore until a signal
becomes available.  Once a signal becomes available, the function will return
the information for that signal to the caller.

In addition to the wait/no-wait option, there is also an option of which
signal of a set the user wants to read.  If there is more than one signal of
the specified type available in the database, the user can elect to read
either the "oldest" or the "newest" signal in the database.  The "oldest"
signal is the one that was received the longest time ago of all of the signals
in the database and the "newest" signal is the one that was received most
recently.

Note that fetching the newest signal of a set will not result in that signal
being removed from the VSI database.  Fetching the oldest signal of a set will
result in that signal being removed from the database.

#### vsi_fire_signal

```C
    int vsi_fire_signal ( vsi_handle handle, vsi_result* result );

    int vsi_fire_signal_by_name ( vsi_handle handle, vsi_result* result );
```

These functions will allow an application to generate a new signal into the
VSI database.  The signal can be specified with either it's name or its domain
and ID.  If the name form is used, the specified name must be defined in the
VSS specification for this signal set, otherwise, an error will be generated.

These calls will copy the supplied information into the VSI database and if
another thread or process is waiting on that signal, processing will resume
for that thread or process.  This call will return to the caller immediately
under all circumstances with the return value indicating the status of the
operation.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored
in.  This is currently defined as a small positive integer value.

The signalId is a numeric value which will identify the specific signal
desired by the caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

The data length parameter is the size of the user's data buffer being
supplied.  This is the maximum number of bytes of data that will be copied
into the supplied buffer by this code.  This value should be set to the
maximum size of the data buffer being supplied.  If the data buffer supplied
is not large enough to contain the entire data record, the data will be
truncated to fix into the buffer.

The data pointer is the address of the buffer in which the record data will be
copied.  Note that the data is copied as binary data and will not
automatically have a null appended to it if it is actually ASCII data.  It is
the caller's resonsibility to ensure that if the data being retrieved is ASCII
data, that those strings are properly null terminated.

#### vsi_get_oldest_signal

```C
    int vsi_get_oldest_signal ( vsi_handle handle, vsi_result* result );

    int vsi_get_oldest_signal_by_name ( vsi_handle handle, vsi_result* result );
```

These functions read the signal events from the oldest entry to the newest
entry by ID or name.

The calling application is responsible for providing a vsi_result structure as
input and for providing both a pointer to a valid data buffer and the length of
the buffer as information in the structure. The API will write data to the
buffer and overwrite the length field with the number of bytes written. If the
API detects an error specific to the signal, it will log an error in the status
field of the structure.

If two or more signals have been generated since the last read, the oldest
signal will be read first. Once the oldest signal has been read, the API will
handle the clean up such that the message cannot be read a second time by the
same application. Calling applications may continue to consume data by issuing
additional calls to vsi_get_oldest_signal until they reach the end of the
buffer, where an error will be returned to indicate that there is no more data.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored
in.  This is currently defined as a small positive integer value.

The signalId is a numeric value which will identify the specific signal
desired by the caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

The calling application is responsible for providing a vsi_result structure as
input and for providing both a pointer to a valid data buffer and the length
of the buffer as information in the structure. The API will write data to the
buffer and overwrite the length field with the number of bytes written into
the buffer. If the API detects an error specific to the signal, it will log an
error in the status field of the structure.

#### vsi_get_newest_signal

```C
    int vsi_get_newest_signal ( vsi_handle handle,
                                vsi_result*  result );

    int vsi_get_newest_signal_by_name ( vsi_handle handle,
                                        vsi_result*  result );
```

These functions will retrieve the latest signal of the specified domain and ID
or name from the core database.  If no signal of the specified type is
available when the call is made, the functions will suspend execution until a
signal is available.  At that point, the information for that signal will be
returned to the caller in the result object.

Unlike the vsi_get_oldest_signal function, this call does not remove a message
from the core database.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored
in.  This is currently defined as a small positive integer value.

The signalId is a numeric value which will identify the specific signal
desired by the caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

The calling application is responsible for providing a vsi_result structure as
input and for providing both a pointer to a valid data buffer and the length
of the buffer as information in the structure. The API will write data to the
buffer and overwrite the length field with the number of bytes written into
the buffer. If the API detects an error specific to the signal, it will log an
error in the status field of the structure.

#### vsi_flush_signal

```C
    int vsi_flush_signal ( vsi_handle     handle,
                           const domain_t domainId,
                           const signal_t signalId );

    int vsi_flush_signal_by_name ( vsi_handle  handle,
                                   const char* name );
```

This function flushes all unread messages from the VSI core database for the
signal with the specified domain and ID or name.

If an application is no longer interested in any of the pending signals to be
read or simply wants to synchronize with the latest information, it may call
this function to flush all pending messages from the core database.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored
in.  This is currently defined as a small positive integer value.

The signalId is a numeric value which will identify the specific signal
desired by the caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

### Groups of Signals
There are applications which need to operate on a set of signals as a group.
For instance, an application may want to wait for any one of a set of signals
to occur or it may want to wait for all of a set of signals to occur.  Both of
these operations can be accommodated by creating a "group" containing the set
of signals desired and then operating on that group instead of the individual
signals.

The basic procedure is to create a new group and add the signals, one at a
time, to that group.  Once all of the signals have been added to the group,
the group can be operated on as an aggregate of all of the signals it
contains.

#### vsi_create_signal_group

```C
    int vsi_create_signal_group ( vsi_handle    handle,
                                  const group_t groupId );
```
This function creates a signal group with the specified group ID local to the
calling application.  Note that groups are not visible to other tasks
connected to the VSI module.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is a numeric value that the caller wants to assign to this group.
The value can be anything that is not currently in use within the possible
range of value for this data type.

#### vsi_delete_signal_group

```C
    int vsi_delete_signal_group ( vsi_handle    handle,
                                  const group_t groupId);
```
This function deletes a signal group with the specified group ID.  All of the
signals associated with that group will be removed and the group itself
destroyed.  References to the group ID after it has been deleted are invalid
and will result in undefined behavior.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is the numeric group ID previously assigned to the group being
operated on.

#### vsi_add_signal_to_group

```C
    int vsi_add_signal_to_group ( vsi_handle     handle,
                                  const domain_t domainId,
                                  const signal_t signalId,
                                  const group_t  groupId );

    int vsi_add_signal_to_group_by_name ( vsi_handle    handle,
                                          const char*   name,
                                          const group_t groupId );
```
Add a signal to the specified group ID by domain and signal IDs or name.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored in.
This is currently defined as a small positive integer value.

The signalId is a value which will identify the specific signal desired by the
caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

The groupId is the numeric group ID previously assigned to the group being
operated on.

#### vsi_remove_signal_from_group

```C
    int vsi_remove_signal_from_group ( vsi_handle     handle,
                                       const domain_t domainId,
                                       const signal_t signalId,
                                       const group_t  groupId );

    int vsi_remove_signal_from_group_by_name ( vsi_handle    handle,
                                               const char*   name,
                                               const group_t groupId);
```
This function removes a signal from the specified group using the signal
domain and ID values or the signal name.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored in.
This is currently defined as a small positive integer value.

The signalId is a value which will identify the specific signal desired by the
caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

The groupId is the numeric group ID previously assigned to the group being
operated on.

#### vsi_get_newest_in_group

```C
    int vsi_get_newest_in_group ( vsi_handle    handle,
                                  const group_t groupId,
                                  vsi_result*   result );

    int vsi_get_newest_in_group_wait ( vsi_handle    handle,
                                       const group_t groupId,
                                       vsi_result*   result )
```

Retrieve the newest signal for every member of the specified group. The
behavior of the function differs depending on whether or not the "wait" option
is used.

In the no wait case, the newest member of each signal in the specified group
will be retrieved and stored in the result array.  If one or more of the
signals do not have any members then the "ENODATA" status condition will be
stored in the result structure for that particular signal.  Once all of the
signals have been checked, this function returns to the caller without waiting.

In the wait case, the newest member of each signal in the specified group will
be retrieved and stored in the result array.  If any of the signals in the
group do not contain any members, execution will be suspended for the current
thread until some data is received for that member.  When execution resumes,
the next signal will be fetched and so forth until all of the signals have
been retrieved.  At that point this function will return to the caller.

WARNING: The "\_wait" version of this function is not currently implemented!

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is the numeric group ID previously assigned to the group being
operated on.

In the case of this group fetch function, the result pointer is not a pointer
to a single vsi_result structure instance, it is a pointer to an array of
vsi_result structure instances.  The number of data structures must be at least
as large as the number of signals defined in the specified group.  Failure to
allocate enough space for the return data will result in memory corruption and
undefined behavior (almost always very bad!).

For both of these functions, the signal data that is retrieved and returned to
the caller will NOT be removed from the VSI core database once it is copied
into the result structures.

#### vsi_get_oldest_in_group

```C
    int vsi_get_oldest_in_group ( vsi_handle    handle,
                                  const group_t groupId,
                                  vsi_result*   result );

    int vsi_get_oldest_in_group_wait ( vsi_handle    handle,
                                       const group_t groupId,
                                       vsi_result*   result );
```
Retrieve the oldest signal for every member of the specified group. The
behavior of the function differs depending on whether or not the "wait" option
is used.

In the no wait case, the oldest member of each signal in the specified group
will be retrieved and stored in the result array.  If one or more of the
signals do not have any members then the "ENODATA" status condition will be
stored in the result structure for that particular signal.  Once all of the
signals have been checked, this function returns to the caller without waiting.

WARNING: The ```_wait``` version of this function is not currently implemented!

In the wait case, the oldest member of each signal in the specified group will
be retrieved and stored in the result array.  If any of the signals in the
group do not contain any members, execution will be suspended for the current
thread until some data is received for that member.  When execution resumes,
the next signal will be fetched and so forth until all of the signals have
been retrieved.  At that point this function will return to the caller.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is the numeric group ID previously assigned to the group being
operated on.

In the case of this group fetch function, the result pointer is not a pointer
to a single vsi_result structure instance, it is a pointer to an array of
vsi_result structure instances.  The number of data structures must be at least
as large as the number of signals defined in the specified group.  Failure to
allocate enough space for the return data will result in memory corruption and
undefined behavior (almost always very bad!).

For both of these functions, the signal data that is retrieved and returned to
the caller will be removed from the VSI core database once it is copied into
the result structures.

#### vsi_listen_any_in_group

```C
	int vsi_listen_any_in_group ( vsi_handle    handle,
								  const group_t groupId,
								  unsigned int  timeout,
								  domain_t*     domainId,
								  signal_t*     signalId );
```

This function will listen for any signal in the specified group.  The signal
list must have been previously built by adding the desired signals to the group
before making this call.  The first signal received after this call is made
will return it's value and the function will return to the caller.

Unlike many of the other functions which return data for a group, this
function does not expect (nor returns) an array of result structures.  It only
returns a single result for the first signal in the group that contains data.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is the numeric group ID previously assigned to the group being
operated on.

The timeout value is the maximum amount of time the user wants this function
to wait for data.

TODO: WARNING: The "timeout" parameter is currently not implemented!

The domainId is an identifier for which domain this record should be stored in.
This is currently defined as a small positive integer value.

The signalId is a value which will identify the specific signal desired by the
caller within the domain specified.

#### vsi_listen_all_in_group

```C
	int vsi_listen_all_in_group ( vsi_handle    handle,
								  const group_t groupId,
								  vsi_result*   results,
								  unsigned int  resultsSize,
								  unsigned int  timeout );
```

This function will listen for all signals in the specified group.  The signal
list must have been previously built by adding the desired signals to the group
before making this call.  When all of the signals defined in the specified
group have received a signal, this call will return to the caller.  This
function will hang until all of the specified signals have been received.

Note that the results pointer argument here MUST be a pointer to an array of
vsi_result structures and that array must be at least as large as the number of
signals that are expected.  The resultsSize argument is the number of
structures being supplied (NOT the total size of the results array in bytes!).
If the resultsSize is not large enough to hold all of the results required, an
error will be returned to the caller.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is the numeric group ID previously assigned to the group being
operated on.

The results parameter is a pointer to an array of vsi_result data structures.

The resultsSize parameter is the number of structures in the "results" array.

The timeout value is the maximum amount of time the user wants this function
to wait for data.

TODO: WARNING: The "timeout" parameter is currently not implemented!

#### vsi_flush_group

```C
    int vsi_flush_group ( vsi_handle    handle,
                          const group_t groupId );
```

This function flushes all unread messages from the VSI message queue for each
signal in the group.

If an application is no longer interested in any of the pending signals to be
read or simply wants to synchronize with the latest information, it may call
this function to flush all pending messages from the queues of every signal in
the group.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The groupId is the numeric group ID previously assigned to the group being
operated on.

### Miscellaneous Functions

#### Signal ID Conversion Functions
The following functions convert from domainId/signalId to signal name and
vice versa.

```C
    int vsi_name_string_to_id ( vsi_handle  handle,
                                const char* name,
                                domain_t*   domainId,
                                signal_t*   signalId );

    int vsi_signal_id_to_string ( vsi_handle     handle,
                                  const domain_t domainId,
                                  const signal_t signalId,
                                  char**         name,
                                  unsigned int   nameLength );

	int vsi_define_signal_name ( vsi_handle     handle,
								 const domain_t domainId,
								 const signal_t signalId,
								 const char*    name );
```

The vsi_name_string_to_id function takes a signal name and returns the domain
and signal ID that corresponds to that name.  The domain/ID mapping must have
been previously defined with a call to the vsi_define_signal_name function
above for this function to work properly.

The vsi_signal_id_to_string function is the inverse of the previous function.
It takes the signal domain/ID and returns the signal name that corresponds to
that signal.  The domain/ID mapping must have been previously defined with a
call to the vsi_define_signal_name function above for this function to work
properly.

The vsi_define_signal_name function allows the user to define the mapping
between a signal domain/ID and it's name.  This will produce a 2-way mapping
to allow the above functions to operate.

The handle parameter is the handle to the current VSI database context that
was returned by the vsi_initialize call.

The domainId is an identifier for which domain this record should be stored
in.  This is currently defined as a small positive integer value.

The signalId is a numeric value which will identify the specific signal
desired by the caller within the domain specified.

The name is an ASCII character string that has been defined in the VSS and
associated with a domain and a numeric signal ID value.

The nameLength is the size of the supplied name buffer in bytes.

In the case of the ID to string conversion, the name pointer must be a
pointer to a pointer to a name string.  The address of the name will be stored
into this pointer variable and null terminated.


## Building...
The API code in this repository comes bundled with a sample application for ease
of understanding. By default, both the VSI Application library and the sample
progam are built:

    make

or

    make all

or
    make rebuild

This last call will rebuild all targets, regardless of whether they have been
changed or not since the last build.  It essentially does a ```make distclean```
followed by a ```make all```.

### The API
Compiling the API library is straightforward:

    make api

### The Sample Applications
The ```sample``` application is a simple example application and can be built
as a separate make target:

    make sample

The ```sample``` program can be run by simply typing:

    sample

The ```importVSS``` application is another sample application that reads the
specified Vehicle Signal Specification file and populates the VSI database
with the signal names and Id values.  After this is run, signals can be
referenced by using either their VSS names or their numeric domain and signal
IDs.

By default, ```make all``` will compile the API library, the ```sample```
application and the ```importVSS``` application.

### The Documentation
Doxygen is used to generate the human-readable documentation from the [API
header file](vsi.h). To generate this documentation, run:

    make docs

The output will be in both HTML and LaTeX formats in subdirectories called
"html" and "latex".

To access the HTML docs, open the ./html/index.html file in a program capable
of rendering HTML data such as a web browser.

To access the LaTeX docs, see the "latex" directory.

### Cleaning Up
All of the object files, libraries, and executables can be removed, setting
the directory back to it's initial state by:

    make clean

or

    make distclean

Both forms are equivalent.

## Notes:
All data flows from producers to consumers. All communication from a consumer to
a producer (for example, an application configuring the HVAC) will be through
CommonAPI/D-Bus/RPCnotyetinvented. This is to prevent producers from having to
share any control for the firing of signals.

The calling application is responsible for passing in buffers and the length
of the buffers for the API to store the resultant data in.

## Future Considerations
1. Additional helper functions for quality of life improvements.
2. Security controls to restrict what signals an application can consume.
3. Implementation of some of the deferred function noted above.
