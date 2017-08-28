/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

    @file signals.h

    This file contains the data structures and function prototypes defined for
    the VSI signals.

    This defines the interface that users of the VSI are expected to conform
    to.

    This file should be included by all applications intending to either
    produce or consume signals via the Vehicle Signal Interface. This file is
    responsible for exposing all available function prototypes and data types.

-----------------------------------------------------------------------------*/

#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include "vsi.h"
#include "sharedMemoryLocks.h"


extern vsi_context* vsiContext;


/*!----------------------------------------------------------------------------

    C o n f i g u r a t i o n   S e t t i n g s

    @brief Define the system configuration settings.

    The following definitions should be customized to suit the user's specific
    environment.

    Note that the "#define" symbols are all conditionally set here to allow
    the user to define these in his environment if he desires.

-----------------------------------------------------------------------------*/
//
//  Define the name of the VSS input file.  This file will be read during the
//  initialization of the VSI system.  Note that users may need to supply a
//  pathname for this file if it is not located in the same directory as that
//  from which the executable is being run.
//
#ifndef VSS_INPUT_FILE
#    define VSS_INPUT_FILE "../vss/vss_rel_1.0.vsi"
#endif


//
//  The following macros are defined to make it easier to call the HexDump
//  function with various combinations of arguments.
//
void HexDump ( const char *data, int length, const char *title,
               int leadingSpaces );

#define HEX_DUMP(   data, length )         HexDump ( data, length, "", 0 )
#define HEX_DUMP_T( data, length, title )  HexDump ( data, length, title, 0 )
#define HEX_DUMP_L( data, length, spaces ) HexDump ( data, length, "", spaces )


/*!-----------------------------------------------------------------------

    s i g n a l _ l i s t

    @brief Define the signal list structure.

    This will define the structure of each signal list in the system.  A
    signal list is a list of signals that all have the same domain and signal
    values and therefore should be kept together.

    The main data structure of the user shared memory system is a B-tree data
    struture that contains the offsets of these signal list structures.  This
    allows us to quickly store/retrieve the signal list for a given domain and
    signal value and allows an unlimited number of unique domain/signal
    combinations to be stored in the system.

------------------------------------------------------------------------*/
typedef struct signal_list
{
    //
    //  Define the signal domain and ID values for all of the signals that
    //  will be stored in this signal list.
    //
    domain_t  domainId;
    signal_t  signalId;
    private_t privateId;
    name_t    name;

    //
    //  Define the variables that will be used to keep track of the linked
    //  list data within each signal list.  This is a list of all of the data
    //  records that have the same domain and signal values.
    //
    //  Note: The "head" is the offset to the first available message in the
    //  current list and the "tail" is the offset to the last available
    //  message in the list. This is required because we need to be able to
    //  update the "next" pointer in the last message whenever we append a new
    //  message to the list and we can't back up in the list (it's a singly
    //  linked list).
    //
    offset_t head;
    offset_t tail;

    //
    //  The following fields are just for informational use.  We probably
    //  don't need them but they could come in handy.  The total size field is
    //  the sum of all of the message fields in this list but this does not
    //  count the signal data header structure sizes in the sum.
    //
    unsigned long currentSignalCount;
    unsigned long totalSignalSize;

    //
    //  Define the semaphore that will be used to manage the processes waiting
    //  for signals on the message queue.  Each signal that is received will
    //  increment the semaphore and each "wait" that is executed will
    //  decrement the semaphore.
    //
    //  TODO: This was changed to a condition variable a while ago but most of
    //  the text still calls it a "semaphore" - Fix it!
    //
    semaphore_t semaphore;

}   signal_list;

#define SIGNAL_LIST_SIZE   ( sizeof(signal_list) )
#define END_OF_LIST_MARKER ( 0 )


/*!-----------------------------------------------------------------------

    s i g n a l D a t a
TODO: REDO!
    @brief Define the structure of the header of each signal data item.

    This message header contains all of the information required to manage a
    message in the shared memory structures.  This "shared message" structure
    is inherently variable in size, depending on how much "data" is being
    stored by the calling user.  This message header will be prepended to the
    data that the user asked us to store.

    The "signal_t" field is the primary indexing value for this message.  It
    is defined here as an unsigned 64 bit numberic value.  Keys must be unique
    within a domain for all messages that represent the same type of data.

    The "domain" is a small integer used to identify the domain of a signal.
    The domain is intended as an identifier that will be the same for all
    messages that belong to that domain.  For instance, all messages coming
    from the CAN bus will be in the CAN domain even though there may be 1500
    or more distinct types of CAN messages.  Each distinct type within the CAN
    domain is identified by a unique "signal" value.

    The "messageSize" field is the number of bytes of data that is being
    stored in the "data" field of this structure.  This value does not include
    this message header size, just the amount of data stored in this record.

    The "nextMessageOffset" is the offset within this signal list where the
    "next" message in a message list is stored.  Traversing down the list from
    the signal list "head" field will allow all of the messages in this hash
    bucket to be found.  The end of the message list is marked with a special
    marker defined above.

    The "waitCount" is used when a new message is being inserted into the
    message list.  The number of processes waiting for this message will be
    copied into this field (from the semaphore count field) when the message
    is inserted.  All of the waiting processes will then be released (via a
    "broadcast" condition variable call) and each of them will then be able to
    retrieve the new message when they become scheduled to run.  When one of
    those processes fetches the new message, the message will not be removed
    from the message list, but the "waitCount" will be decremented by one each
    time the message is copied for a process.  Only after the "waitCount" has
    been decremented to zero will the message actually be removed from the
    message list, thereby ensuring that all processes that were waiting for
    this message have gotten a copy of the message before the message is
    removed from the message list.

    The "data" field is where the actual data that the user has asked us to
    store will be copied.  This is an array of bytes whose size depends on the
    "messageSize" that the caller has specified.

------------------------------------------------------------------------*/
typedef struct signal_data
{
    offset_t nextMessageOffset;
    offset_t messageSize;
    char     data[0];

}   signal_data;

//
//  Define the size of the shared message header structure.  This value needs
//  to be taken into account when the total size of a message is computed.
//  The "messageSize" field in this header is the size of the "data" field at
//  the end of the structure so to get the total size of a structure, we must
//  use:
//
//    totalMessageSize = messagePtr->messageSize + SIGNAL_DATA_HEADER_SIZE;
//
#define SIGNAL_DATA_HEADER_SIZE ( sizeof(signal_data) )


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G e n e r a t i o n
    =========================================


    v s i _ i n s e r t _ s i g n a l

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

    @param[in] - result - The vsi_result object that will contain the
                          information needed to insert the signal.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_insert_signal ( vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ i n s e r t _ s i g n a l _ b y _ n a m e

    @brief Fire a vehicle signal by its name.

    This function is identical to the vsi_insert_signal except that the desired
    signal is specified by the signal's ASCII name rather than it's domain and
    ID values.

    The following fields must be set in the result data structure:

    The name of the signal being inserted.

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

    @param[in] - result - The vsi_result object that will contain the
                          information needed to insert the signal.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_insert_signal_by_name ( vsi_result* result );


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

    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_signal ( vsi_result* result );


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

    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_signal_by_name ( vsi_result* result );


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

    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_signal ( vsi_result* result );


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

    @param[in/out] - result - The address of the result structure.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_signal_by_name ( vsi_result* result );


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l

    @brief Flush all signals by domain and ID.

    This function flushes all unread messages from the VSI core database for
    the signal with the specified domain and ID.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the core database.

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a numeric value which will identify the specific signal
    desired by the caller within the domain specified.

    @param[in] - domainId - The domain ID.
    @param[in] - signalId - The signal ID.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_flush_signal ( const domain_t domainId,
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

    The name is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    @param[in] - name - The name of the signal to flush.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_flush_signal_by_name ( const domain_t domain, const char* name );


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
typedef struct vsi_signal_group_data
{
    offset_t nextMessageOffset;
    offset_t signalList;

}   vsi_signal_group_data;


/*!-----------------------------------------------------------------------

    s t r u c t   v s i _ s i g n a l _ g r o u p

    @brief This data structure manages lists of signals in a group.

    This defines the data structure that keeps track of groups and the signals
    that belong to each group.  This catagorization is accomplished with
    another btree that is indexed by the group Id.  Each group then contains a
    list of all of the signals that have been registered for that group.

    TODO: Add mutex locks to group manipulation functions.

------------------------------------------------------------------------*/
typedef struct vsi_signal_group
{
    group_t         groupId;
    int             count;
    offset_t        head;      // Offset to a vsi_signal_group_data record
    offset_t        tail;      // Offset to a vsi_signal_group_data record
    pthread_mutex_t mutex;

}   vsi_signal_group;


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G r o u p   F u n c t i o n s
    ===================================================


    v s i _ c r e a t e _ s i g n a l _ g r o u p

    @brief Create a new signal group.

    This function will create a new signal group with the specified ID value.
    Note that groups are not visible to other tasks connected to the VSI
    module.

    The groupId is a numeric value that the caller wants to assign to this
    group.  The value can be anything that is not currently in use within the
    possible range of value for this data type.

    @param[in] - groupId - The ID value desired for this new group.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_create_signal_group ( const group_t groupId );


/*!-----------------------------------------------------------------------

    v s i _ d e l e t e _ s i g n a l _ g r o u p

    @brief Delete the specified signal group from the system.

    This function will delete the specified signal group from the system.

    @param[in] - groupId - The ID value of the group to be deleted.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_delete_signal_group ( const group_t groupId);


/*!-----------------------------------------------------------------------

    v s i _ f e t c h _ s i g n a l _ g r o u p

    @brief Fetch the specified signal group record.

    This function will find the specified signal group data structure and
    return a pointer to that structure.

    @param[in] - groupId - The ID of the group desired.

    @return The address of the signal group data structure found
            NULL if the specified signal group could not be found.

------------------------------------------------------------------------*/
vsi_signal_group* vsi_fetch_signal_group ( const group_t groupId );


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p

    @brief Add a new signal to the specified group.

    This function will add a new signal to the specified group.

    The domainId is an identifier for which domain this record should be
    located in.  This is currently defined as a small positive integer value.

    The signalId is a value which will identify the specific signal desired by
    the caller within the domain specified.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    @param[in] - domainId - The domain ID value of the signal to be added.
    @param[in] - signalId - The signal ID value of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function
------------------------------------------------------------------------*/
int vsi_add_signal_to_group ( const domain_t domainId,
                              const signal_t signalId,
                              const group_t  groupId );


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p _ b y _ n a m e

    @brief Add a new signal to the specified group by signal name.

    This function is identical to the vsi_add_signal_to_group above except
    that the signal being added to the group is identified by giving it's name
    rather than by domain and ID.

    @param[in] - name - The name of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_add_signal_to_group_by_name ( const domain_t domainId,
                                      const char*    name,
                                      const group_t  groupId );


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p

    @brief Remove the specified signal from the given group.

    This function will remove a signal, specified by it's domain and ID, and
    remove it from the specified group.

    The domainId is an identifier for which domain this record should be
    stored in.  This is currently defined as a small positive integer value.

    The signalId is a value which will identify the specific signal desired by
    the caller within the domain specified.

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    @param[in] - domainId - The domain ID value of the signal to be added.
    @param[in] - signalId - The signal ID value of the signal to be added.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group ( const domain_t domainId,
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

    @param[in] - name - The name of the signal to be removed.
    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group_by_name ( const domain_t domain,
                                           const char*    name,
                                           const group_t  groupId );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ i n _ g r o u p

    @brief Retrieve the newest signal for every member of the specified group.

    The newest member of each signal in the specified group will be retrieved
    and stored in the result array.  If one or more of the signals do not have
    any members then the "ENODATA" status condition will be stored in the
    result structure for that particular signal.  Once all of the signals have
    been checked, this function returns to the caller without waiting.

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

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_in_group ( const group_t groupId,
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

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_newest_in_group_wait ( const group_t groupId,
                                   vsi_result*   result );


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p

    @brief Retrieve the oldest signal for every member of the specified group.

    The oldest member of each signal in the specified group will be retrieved
    and stored in the result array.  If one or more of the signals do not have
    any members then the "ENODATA" status condition will be stored in the
    result structure for that particular signal.  Once all of the signals have
    been checked, this function returns to the caller without waiting.

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

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group ( const group_t groupId,
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

    @param[in] - groupId - The ID value of the group to be modified.
    @param[in/out] - result - The array of structures that will hold the data.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group_wait ( const group_t groupId,
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

    @param[in] groupId - The address of where to store the group ID
    @param[in] timeout - The optional timeout value in nanoseconds
    @param[out] domainId - The address of where to store the domain ID
    @param[out] signalId - The address of where to store the signal ID

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
int vsi_listen_any_in_group ( const group_t groupId,
                              unsigned int  timeout,
                              vsi_result*   result );


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

    @param[out] resultsPtr - The address of where to store the results
    @param[in] resultsSize - The size of the results array being supplied
                             The number of results expected.
    @param[in] timeout - The optional timeout value in nanoseconds

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            EINVAL - There are no signals or groups to listen to.
            ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/
int vsi_listen_all_in_group ( const group_t groupId,
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

    The groupId is the numeric group ID previously assigned to the group being
    operated on.

    @param[in] - groupId - The ID value of the group to be modified.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_flush_group ( const group_t groupId );


/*!-----------------------------------------------------------------------

    N a m e / I D   M a n i u p l a t i o n   F u n c t i o n s
    ===========================================================


    v s i _ n a m e _ s t r i n g _ t o _ i d

    @brief Convert a signal name to it's domain and signal ID values.

    The name is an ASCII character string that has been defined in the VSS and
    associated with a domain and a numeric signal ID value.

    The domainId is a pointer to an identifier in which the signal domain will
    be stored.

    The signalId is a pointer to an identifier in which the signal ID will
    be stored.

    @param[in] - name - The name of the signal to be converted.
    @param[in] - domainId - The domain associated with the name.
    @param[out] - signalId - A pointer in which the signal ID will be stored.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_name_string_to_id ( domain_t    domainId,
                            const char* name,
                            signal_t*   signalId );


/*!-----------------------------------------------------------------------

    v s i _ s i g n a l _ i d _ t o _ s t r i n g

    @brief Convert a signal domain and ID to it's ASCII string name.

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

    @param[in] - domainId - The signal domain ID to be converted.
    @param[in] - signalId - The signal ID to be converted.
    @param[in/out] - name - A pointer to a buffer in which to store the name.
    @param[in] - nameLength - The size of the name buffer being supplied.

    @return - status - The return status of the function

------------------------------------------------------------------------*/
int vsi_signal_id_to_string ( const domain_t domainId,
                              const signal_t signalId,
                              char**         name,
                              int            nameLength );


/*!-----------------------------------------------------------------------

    v s i _ d e f i n e _ s i g n a l _ n a m e

    @brief Define new signal Id/Name definition record.

    This function will define a new signal Id/Name definition record.  This
    record will be inserted into the signal Id and signal Name btree indices
    to allow conversions between the Id and Name of a signal and vice versa.

    @param[in] domainId - The signal domain ID to be defined.
    @param[in] signalId - The signal ID to be defined.
    @param[in] privateId - The private signal ID associated with this signal
    @param[in] name - The ASCII name of the signal to be defined.

    @return None

------------------------------------------------------------------------*/
int vsi_define_signal ( const domain_t domainId,
                        const signal_t signalId,
                        const signal_t privateId,
                        const char*    name );

//
//  Declare the signal dump functions.
//
extern void dumpSignals            ( void );
extern void signalTraverseFunction ( char* leader, void* dataPtr );
extern void printSignalList        ( signal_list* signalList, int maxSignals );
extern void printSignalData        ( signal_list* signalList, int maxSignals );

extern void printResult            ( vsi_result* result, const char* text );

//
//  Declare the signal group dump function.
//
extern void dumpGroups            ( void );
extern void groupTraverseFunction ( char* leader, void* dataPtr );
extern void printSignalGroup      ( char* leader, void* data );

//
//  Declare the old shared memory utility functions.
//
int sm_insert ( domain_t domain, signal_t signal, unsigned long
                newMessageSize, void* body );

int sm_removeSignal ( signal_list* signalList );

int sm_fetch ( domain_t domain, signal_t signal, unsigned long* bodySize,
               void** body, bool wait );

int sm_fetch_newest ( domain_t domain, signal_t signal, unsigned long*
                      bodySize, void** body, bool wait );

int sm_flush_signal ( domain_t domain, signal_t signal );


#endif  //  _SIGNALS_H_

/*! @} */

// vim:filetype=h:syntax=c
