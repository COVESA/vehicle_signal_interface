/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

    @file vsi.h

    This file contains the data structures and function prototypes defined in
    the VSI API.

    This defines the interface that user's of the VSI are expected to conform
    to.

    This file should be included by all applications intending to either produce
    or consume signals via the Vehicle Signal Interface. This file is
    responsible for exposing all available function prototypes and data types.

    @author Matthew Vick &lt;mvick@jaguarlandrover.com&gt;
    @author Substantially modified by Don Mies &lt;dmies@jaguarlandrover.com&gt;

-----------------------------------------------------------------------------*/

#ifndef _VSI_H_
#define _VSI_H_

#include "vsi_core_api.h"
#include "vsi_list.h"
#include "btree.h"


/*!-----------------------------------------------------------------------

    D a t a   T y p e s

    @brief Define the basic types for the signal data.

------------------------------------------------------------------------*/
typedef unsigned int domain_t;
typedef unsigned int signal_t;
typedef unsigned int group_t;

//
//  Application handle used to interact with the API.
//
typedef void* vsi_handle;


/*!-----------------------------------------------------------------------

	s t r u c t   v s i _ c o n t e x t

	@brief The master handle structure that gets passed to all VSI functions

	This data structure is the "handle" object that gets passed to all of the
    VSI functions.  All of the master data objects in the VSI system are
    defined as members of this structure.

    This data structure also contains the handle to the VSI core system which
    is also an opaque type.

------------------------------------------------------------------------*/
typedef struct vsi_context
{
    //
    //  Define the handle to the VSI core internal structures.
    //
    vsi_core_handle coreHandle;

    //
    //  Define the btree indices which will be used to look up the IDs and
    //  names of signals.  Each domain/signal id and it's corresponding name
    //  will be added to both of these indices to enable lookups in either
    //  direction.
    //
    btree* signalNameIndex;
    btree* signalIdIndex;

    //
    //  Define the btree index that will be used to keep track of the groups
    //  in the system.
    //
    btree* groupIdIndex;

}   vsi_context;


/*!-----------------------------------------------------------------------

    s t r u c t   v s i _ r e s u l t

    @brief Used to pass data to and return data from the API.

    The contents of this structure must be initialized with the information
    required by the function being called as input (often the domain and
    signal ID or the signal name) and the results of the function execution
    will be returned in the structure upon exit.

    Note that when the data and dataLength fields are required, the caller is
    responsible for allocating a memory area large enough to contain the data
    being asked for.  The dataLength parameter then specifies the size of that
    allocated memory area.  Many of the functions that return data in these
    fields will replace the dataLength value with the actual number of bytes
    written into the data buffer before returning to the caller.

    Return data will always be written with the minimum of either the size of
    the data being returned or the size of the supplied buffer, whichever one
    is smallest.  No data in the buffers past the actual returned data will be
    changed, that is, the buffer will not be padded with any data to fill it
    out to it's specified size.

    This structure contains a mutex so that the threads can lock this
    structure while reading or writing their results into it without being
    interrupted by another thread trying to do the same thing... First one in
    the pool wins!

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The signal name is an ASCII string that uniquely identifies the signal in
    question.

    The data pointer is the address of the buffer which constains the record
    data.  Note that the data is not assumed to be ASCII in nature so it is
    the caller's responsibility to ensure that it is null terminated if need
    be.

    The dataLength parameter is the size of the user's data buffer being
    supplied.  This is the maximum number of bytes of data that will be copied
    into the supplied buffer by the API code.  This value should be set to the
    maximum size of the data buffer being supplied.  If the data buffer
    supplied is not large enough to contain the entire data record, the data
    will be truncated to fix into the buffer.

    Generally speaking, the "result" pointer defined in the functions that
    follow is actually a pointer to an array of "vsi_result" structures.  If
    the operation only operates on a single signal then this is an array of
    length 1 or simply a single structure.  If the function being used can
    return data for multiple signals then this pointer is actually a pointer
    to an array of "vsi_result" structures where the number of structures must
    be at least as large as the maximum number of signals that could be
    returned by the call.

    The status parameter will contain the return status of the function that
    was called.  For operations on more than one result structure, the status
    field is set individually for each result structure to reflect any errors
    that may have been encountered during the propressing of that structure.

    The caller is responsible for allocating and deallocating all of these
    structures and buffers.

------------------------------------------------------------------------*/
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


/*!-----------------------------------------------------------------------

    s t r u c t   v s i _ i d _ n a m e _ d e f i n i t i o n

	@brief This data structure is designed to hold the Id and name of each
    signal defined in the system.

    This defines the "user data" structure that will be used to build the
    btree Id and name indices.  Those indices will be used to lookup the name
    of a signal given it's domain and signals ids and vice versa.

    We also define some constants and macros related to the btrees that will
    be created from this data.

------------------------------------------------------------------------*/
typedef struct vsi_id_name_definition
{
    domain_t domainId;
    signal_t signalId;
    char*    name;

}   vsi_id_name_definition;


#define VSI_NAME_ID_BTREE_ORDER ( 100 )


/*!-----------------------------------------------------------------------

    s t r u c t   v s i _ s i g n a l _ g r o u p

	@brief This data structure manages lists of signals in a group.

	This defines the data structure that keeps track of groups and the signals
    that belong to each group.  This catagorization is accomplished with
    another btree that is indexed by the group Id.  Each group then contains a
    list of all of the signals that have been registered for that group.

------------------------------------------------------------------------*/
typedef struct vsi_signal_group
{
    group_t  groupId;
    vsi_list list;

}   vsi_signal_group;


#define VSI_GROUP_BTREE_ORDER ( 100 )


/*!-----------------------------------------------------------------------

    S t a r t u p   a n d   S h u t d o w n
    =======================================


    v s i _ i n i t i a l i z e

    @brief Initialize the VSI API.

    This function must be the first function called by applications interested
    in vehicle signals.  The returned vsi_handle object must be supplied as an
    argument to all of the VSI API functions.

    If this function fails for some reason, the returned value will be NULL
    and errno will reflect the error that occurred.

    @param[in] None

    @return vsi_handle - The VSI handle object representing this VSI context.

------------------------------------------------------------------------*/
vsi_handle vsi_initialize ();


/*!-----------------------------------------------------------------------

    v s i _ d e s t r o y

    @brief Destroy an existing VSI context.

    This function will clean up and deallocate all resources utilized by the
    VSI API and it should always be called once the VSI API is no longer
    required.  Once this function is called, the vsi_handle will no longer be
    valid and utilizing it or calling any of the VSI API functions will result
    in undefined behavior.

    @param[in] - handle - The VSI context handle

    @return - status - The function return status value.

------------------------------------------------------------------------*/
int vsi_destroy ( vsi_handle handle );


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G e n e r a t i o n
    =========================================


    v s i _ f i r e _ s i g n a l

    @brief Fire a vehicle signal by its ID.

    These functions will allow an application to generate a new signal into
    the VSI database.  The signal can be specified with either it's name or
    its domain and ID.  If the name form is used, the specified name must be
    defined in the VSS specification for this signal set, otherwise, an error
    will be generated.

    These calls will copy the supplied information into the VSI database and
    if another thread or process is waiting on that signal, processing will
    resume for that thread or process.  This call will return to the caller
    immediately under all circumstances with the return value indicating the
    status of the operation.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The following fields must be set in the result data structure:

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The data length parameter is the size of the user's data buffer being
    supplied.  This is the maximum number of bytes of data that will be copied
    into the supplied buffer by this code.  This value should be set to the
    maximum size of the data buffer being supplied.  If the data buffer
    supplied is not large enough to contain the entire data record, the data
    will be truncated to fix into the buffer.

    The data pointer is the address of the buffer in which the record data
    will be copied.  Note that the data is copied as binary data and will not
    automatically have a null appended to it if it is actually ASCII data.  It
    is the caller's resonsibility to ensure that if the data being retrieved
    is ASCII data, that those strings are properly null terminated.

    TODO: What happens if the specified signal does not exist yet?

    This function returns a 0 upon success and an errno code otherwise.

    @param[in] - handle - Required handle for the API.
    @param[in] - result - The vsi_result object that will contain the
                          information needed to fire the signal.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_fire_signal ( vsi_handle  handle,
                      vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ f i r e _ s i g n a l _ b y _ n a m e

    @brief Fire a vehicle signal by its name.

    This function is identical to the vsi_fire_signal except that the desired
    signal is specified by the signal's ASCII name rather than it's domain and
    ID values.

    The following fields must be set in the result data structure:

    The name of the signal being fired.

    The data length parameter is the size of the user's data buffer being
    supplied.  This is the maximum number of bytes of data that will be copied
    into the supplied buffer by this code.  This value should be set to the
    maximum size of the data buffer being supplied.  If the data buffer
    supplied is not large enough to contain the entire data record, the data
    will be truncated to fix into the buffer.

    The data pointer is the address of the buffer in which the record data
    will be copied.  Note that the data is copied as binary data and will not
    automatically have a null appended to it if it is actually ASCII data.  It
    is the caller's resonsibility to ensure that if the data being retrieved
    is ASCII data, that those strings are properly null terminated.

    @param[in] - handle - Required handle for the API.
    @param[in] - result - The vsi_result object that will contain the
                          information needed to fire the signal.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_fire_signal_by_name ( vsi_handle  handle,
                              vsi_result* result );


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   R e t r i e v a l
    =======================================

    v s i _ g e t _ o l d e s t _ s i g n a l

    @brief Fetch the oldest entry in the core database for a signal.

    This function will find the oldest entry in the core database for the
    specified signal domain and ID and return the data associated with that
    signal to the caller.  The signal data returned will be removed from the
    core database and will no longer be available to anyone.

    The calling application is responsible for providing a vsi_result structure
    as input and for providing both a pointer to a valid data buffer and the
    length of the buffer inside that structure as information in the
    structure. The API will write data to the buffer and overwrite the length
    field with the number of bytes written. If the API detects an error
    specific to the signal, it will log an error in the status field of the
    structure.

    If two or more signals have been generated since the last read, the oldest
    signal will be read first. Once the oldest signal has been read, the
    message will be removed from the core database, thereby "consumming" the
    data.  Calling applications may continue to consume data by issuing
    additional calls to vsi_get_oldest_signal until they reach the end of the
    buffer, where an error will be returned to indicate that there is no more
    data.

    The following fields must be supplied in the result data structure:

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The calling application is responsible for providing a vsi_result structure
    as input and for providing both a pointer to a valid data buffer and the
    length of the buffer as information in the structure. The API will write
    data to the buffer and overwrite the length field with the number of bytes
    written into the buffer. If the API detects an error specific to the
    signal, it will log an error in the status field of the structure.

    @param[in] - handle - Required handle for the API.
    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_signal ( vsi_handle  handle,
                            vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ s i g n a l _ b y _ n a m e

    @brief Fetch the oldest entry in the core database for a signal by name.

    This function is identical to the vsi_get_oldest_signal function above
    except that the signal desired is specified by giving it's ASCII name
    instead of it's domain and ID.

    The following fields must be set in the result data structure:

    The name of the signal being fetched.

    The data length parameter is the size of the user's data buffer being
    supplied.  This is the maximum number of bytes of data that will be copied
    into the supplied buffer by this code.  This value should be set to the
    maximum size of the data buffer being supplied.  If the data buffer
    supplied is not large enough to contain the entire data record, the data
    will be truncated to fix into the buffer.

    The data pointer is the address of the buffer in which the record data
    will be copied.  Note that the data is copied as binary data and will not
    automatically have a null appended to it if it is actually ASCII data.  It
    is the caller's resonsibility to ensure that if the data being retrieved
    is ASCII data, that those strings are properly null terminated.

    @param[in] - handle - Required handle for the API.
    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_signal_by_name ( vsi_handle  handle,
                                    vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ s i g n a l

    @brief Fetch the latest value of the specified signal by ID.

    These functions will retrieve the latest signal of the specified domain
    and ID from the core database.  If no signal of the specified type is
    available when the call is made, the functions will suspend execution
    until a signal is available.  At that point, the information for that
    signal will be returned to the caller in the result object.

    Unlike the vsi_get_oldest_signal function, this call does not remove a
    message from the core database.

    The following fields must be supplied in the result data structure:

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    The calling application is responsible for providing a vsi_result structure
    as input and for providing both a pointer to a valid data buffer and the
    length of the buffer as information in the structure. The API will write
    data to the buffer and overwrite the length field with the number of bytes
    written into the buffer. If the API detects an error specific to the
    signal, it will log an error in the status field of the structure.

    @param[in] - handle - Required handle for the API.
    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_signal ( vsi_handle  handle,
                            vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ s i g n a l _ b y _ n a m e

    @brief Fetch the latest value of the specified signal by name.

    This function is identical to the vsi_get_newest_signal function above
    except that the signal desired is specified by giving it's ASCII name
    instead of it's domain and ID.

    The following fields must be set in the result data structure:

    The name of the signal being retrieved.

    The data length parameter is the size of the user's data buffer being
    supplied.  This is the maximum number of bytes of data that will be copied
    into the supplied buffer by this code.  This value should be set to the
    maximum size of the data buffer being supplied.  If the data buffer
    supplied is not large enough to contain the entire data record, the data
    will be truncated to fix into the buffer.

    The data pointer is the address of the buffer in which the record data
    will be copied.  Note that the data is copied as binary data and will not
    automatically have a null appended to it if it is actually ASCII data.  It
    is the caller's resonsibility to ensure that if the data being retrieved
    is ASCII data, that those strings are properly null terminated.

    @param[in] - handle - Required handle for the API.
    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_signal_by_name ( vsi_handle  handle,
                                    vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l

    @brief Flush all signals by domain and ID.

    This function flushes all unread messages from the VSI core database for
    the signal with the specified domain and ID.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the core database.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    @param[in] - handle - The required handle for the API.
    @param[in] - domainId - The domain ID.
    @param[in] - signalId - The signal ID.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_flush_signal ( vsi_handle     handle,
                       const domain_t domainId,
                       const signal_t signalId );


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l _ b y _ n a m e

    @brief Flush all signals for the given signal name.

    This function flushes all unread messages from the VSI core database for
    the signal with the specified name.

    This function is identical to the vsi_flush_signal function above except
    that the signal desired is specified by giving it's ASCII name instead of
    it's domain and ID.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the core database.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The name is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    @param[in] - handle - The required handle for the API.
    @param[in] - name - The name of the signal to flush.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_flush_signal_by_name ( vsi_handle  handle,
                               const char* name );


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G r o u p   F u n c t i o n s
    ===================================================


    v s i _ c r e a t e _ s i g n a l _ g r o u p

    @brief Create a new signal group.

    This function will create a new signal group with the specified ID value.
    Note that groups are not visible to other tasks connected to the VSI
    module.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The groupId is a numeric value that the caller wants to assign to this
    group.  The value can be anything that is not currently in use within the
    possible range of value for this data type.

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value desired for this new group.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_create_signal_group ( vsi_handle    handle,
                              const group_t groupId );


/*!-----------------------------------------------------------------------

    v s i _ d e l e t e _ s i g n a l _ g r o u p

    @brief Delete the specified signal group from the system.

    This function will delete the specified signal group from the system.

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value of the group to be deleted.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_delete_signal_group ( vsi_handle    handle,
                              const group_t groupId);


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p

    @brief Add a new signal to the specified group.

    This function will add a new signal to the specified group.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The domainId is an identifier for which domain this record should be
    located in.  This is currently defined as a small positive integer value.

    The signalId is a value which will identify the specific signal desired by
    the caller within the domain specified.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    @param[in] - handle - Required handle for the API.
    @param[in] - domainId - The domain ID value of the signal to be added.
    @param[in] - signalId - The signal ID value of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function
------------------------------------------------------------------------*/
int vsi_add_signal_to_group ( vsi_handle     handle,
                              const domain_t domainId,
                              const signal_t signalId,
                              const group_t  groupId );


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p _ b y _ n a m e

    @brief Add a new signal to the specified group by signal name.

    This function is identical to the vsi_add_signal_to_group above except
    that the signal being added to the group is identified by giving it's name
    rather than by domain and ID.

    @param[in] - handle - Required handle for the API.
    @param[in] - name - The name of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_add_signal_to_group_by_name ( vsi_handle    handle,
                                      const char*   name,
                                      const group_t groupId );


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p

    @brief Remove the specified signal from the given group.

    This function will remove a signal, specified by it's domain and ID, and
    remove it from the specified group.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a value which will identify the specific signal desired by
    the caller within the domain specified.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    @param[in] - handle - Required handle for the API.
    @param[in] - domainId - The domain ID value of the signal to be added.
    @param[in] - signalId - The signal ID value of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group ( vsi_handle     handle,
                                   const domain_t domainId,
                                   const signal_t signalId,
                                   const group_t  groupId );


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p _ b y _ n a m e

    @brief  Remove the specified signal from the given group by signal name.

    This function will remove a signal, specified by it's name, and remove it
    from the specified group.

    This function is identical to the vsi_remove_signal_from_group above
    except that the signal being removed from the group is identified by
    giving it's name rather than by domain and ID.

    @param[in] - handle - Required handle for the API.
    @param[in] - name - The name of the signal to be removed.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group_by_name ( vsi_handle    handle,
                                           const char*   name,
                                           const group_t groupId );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ i n _ g r o u p

    @brief Retrieve the newest signal for every member of the specified group.

    The newest member of each signal in the specified group will be retrieved
    and stored in the result array.  If one or more of the signals do not have
    any members then the "ENODATA" status condition will be stored in the
    result structure for that particular signal.  Once all of the signals have
    been checked, this function returns to the caller without waiting.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures must
    be at least as large as the number of signals defined in the specified
    group.  Failure to allocate enough space for the return data will result
    in memory corruption and undefined behavior (almost always very bad!).

    The signal data that is retrieved and returned to the caller will NOT be
    removed from the VSI core database once it is copied into the result
    structures.

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              vsi_result*   result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the newest signal for every member of the specified group.

    The newest member of each signal in the specified group will be retrieved
    and stored in the result array.  If any of the signals in the group do not
    contain any members, execution will be suspended for the current thread
    until some data is received for that member.  When execution resumes, the
    next signal will be fetched and so forth until all of the signals have
    been retrieved.  At that point this function will return to the caller.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures
    must be at least as large as the number of signals defined in the
    specified group.  Failure to allocate enough space for the return data
    will result in memory corruption and undefined behavior (almost always
    very bad!).

    The signal data that is retrieved and returned to the caller will NOT be
    removed from the VSI core database once it is copied into the result
    structures.

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_in_group_wait ( vsi_handle    handle,
                                   const group_t groupId,
                                   vsi_result*   result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p

    @brief Retrieve the oldest signal for every member of the specified group.

    The oldest member of each signal in the specified group will be retrieved
    and stored in the result array.  If one or more of the signals do not have
    any members then the "ENODATA" status condition will be stored in the
    result structure for that particular signal.  Once all of the signals have
    been checked, this function returns to the caller without waiting.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures must
    be at least as large as the number of signals defined in the specified
    group.  Failure to allocate enough space for the return data will result
    in memory corruption and undefined behavior (almost always very bad!).

    The signal data that is retrieved and returned to the caller will be
    removed from the VSI core database once it is copied into the result
    structures

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              vsi_result*   result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the oldest signal for every member of the specified group.

    The oldest member of each signal in the specified group will be retrieved
    and stored in the result array.  If any of the signals in the group do not
    contain any data, execution will be suspended for the current thread
    until some data is received for that signal.  When execution resumes, the
    next signal will be fetched and so forth until all of the signals have
    been retrieved.  At that point this function will return to the caller.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    In the case of this group fetch function, the result pointer is not a
    pointer to a single vsi_result structure instance, it is a pointer to an
    array of vsi_result structure instances.  The number of data structures must
    be at least as large as the number of signals defined in the specified
    group.  Failure to allocate enough space for the return data will result
    in memory corruption and undefined behavior (almost always very bad!).

    The signal data that is retrieved and returned to the caller will be
    removed from the VSI core database once it is copied into the result
    structures

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group_wait ( vsi_handle    handle,
                                   const group_t groupId,
                                   vsi_result*   result );


/*!-----------------------------------------------------------------------

    v s i _ l i s t e n _ a n y _ i n _ g r o u p

    @brief Listen for any signal in the specified group

    This function will listen for any signal in the specified group.  The
    signal list must have been previously built by adding the desired signals
    to the group before making this call.  The first signal received after
    this call is made will return it's value and the function will return to
    the caller.

    Unlike many of the other functions which return data for a group, this
    function does not expect (nor returns) an array of result structures.  It
    only returns a single result for the first signal in the group that
    contains data.

    @param[in] handle - The handle to the current VSI context
    @param[in] groupId - The address of where to store the group ID
    @param[in] timeout - The optional timeout value in nanoseconds
    @param[out] domainId - The address of where to store the domain ID
    @param[out] signalId - The address of where to store the signal ID

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
int vsi_listen_any_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              unsigned int  timeout,
                              domain_t*     domainId,
                              signal_t*     signalId );


/*!-----------------------------------------------------------------------

    v s i _ l i s t e n _ a l l _ i n _ g r o u p

    @brief Listen for all signals in the specified group.

    This function will listen for all signals in the specified group.  The
    signal list must have been previously built by adding the desired signals
    to the group before making this call.  When all of the signals defined in
    the specified group have received a signal, this call will return to the
    caller.  This function will hang until all of the specified signals have
    been received.

    Note that the results pointer argument here MUST be a pointer to an array
    of vsi_result structures and that array must be at least as large as the
    number of signals that are expected.  The resultsSize argument is the
    number of structures being supplied (NOT the total size of the results
    array in bytes!).  If the resultsSize is not large enough to hold all of
    the results required, an error will be returned to the caller.

    @param[in] handle - The handle to the current VSI context
    @param[out] resultsPtr - The address of where to store the results
    @param[in] resultsSize - The size of the results array being supplied
    @param[in] timeout - The optional timeout value in nanoseconds

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
int vsi_listen_all_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              vsi_result*   results,
                              unsigned int  resultsSize,
                              unsigned int  timeout );


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ g r o u p

    @brief Flushes all unread messages from the VSI message queue for each
           signal in the group.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the queues of every
    signal in the group.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    @param[in] - handle - Required handle for the API.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_flush_group ( vsi_handle    handle,
                      const group_t groupId );


/*!-----------------------------------------------------------------------

    N a m e / I D   M a n i u p l a t i o n   F u n c t i o n s
    ===========================================================


    v s i _ n a m e _ s t r i n g _ t o _ i d

    @brief Convert a signal name to it's domain and signal ID values.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The domainId is a pointer to an identifier in which the signal domain will
    be stored.

    The signalId is a pointer to an identifier in which the signal ID will
    be stored.

    @param[in] - handle - Required handle for the API.
    @param[in] - name - The name of the signal to be converted.
    @param[out] - domainId - A pointer in which the domain ID will be stored.
    @param[out] - signalId - A pointer in which the signal ID will be stored.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_name_string_to_id ( vsi_handle  handle,
                            const char* name,
                            domain_t*   domainId,
                            signal_t*   signalId );


/*!-----------------------------------------------------------------------

    v s i _ n a m e _ i d _ t o _ s t r i n g

    @brief Convert a signal domain and ID to it's ASCII string name.

    The handle parameter is the handle to the current VSI database context
    that was returned by the vsi_initialize call.

    The domainId is the domain ID to be converted.

    The signalId is the signal ID to be converted.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The name pointer must be a preallocated memory area that is large enough
    to contain the name of the signal being asked for.  The name of the signal
    will be copied into that buffer and null terminated.

    The nameLength is the maximum number of bytes of data that will be copied
    into the supplied buffer by the API code.  This value should be set to the
    maximum size of the name buffer being supplied.  If the name buffer
    supplied is not large enough to contain the entire name, the data will be
    truncated to fit into the buffer.

    @param[in] - handle - Required handle for the API.
    @param[in] - domainId - The signal domain ID to be converted.
    @param[in] - signalId - The signal ID to be converted.
    @param[in/out] - name - A pointer to a buffer in which to store the name.
    @param[in] - nameLength - The size of the name buffer being supplied.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_name_id_to_string ( vsi_handle     handle,
                            const domain_t domainId,
                            const signal_t signalId,
                            char**         name );


/*!-----------------------------------------------------------------------

    v s i _ d e f i n e _ s i g n a l _ n a m e

    @brief Define new signal Id/Name definition record.

    This function will define a new signal Id/Name definition record.  This
    record will be inserted into the signal Id and signal Name btree indices
    to allow conversions between the Id and Name of a signal and vice versa.

    @param[in] handle - Required handle for the API.
    @param[in] domainId - The signal domain ID to be defined.
    @param[in] signalId - The signal ID to be defined.
    @param[in] name - The ASCII name of the signal to be defined.

    @return None

------------------------------------------------------------------------*/
int vsi_define_signal_name ( vsi_handle     handle,
                             const domain_t domainId,
                             const signal_t signalId,
                             const char*    name );

#endif  //  _VSI_H_
