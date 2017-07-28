/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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

    This file should be included by all applications intending to either
    produce or consume signals via the Vehicle Signal Interface. This file is
    responsible for exposing all available function prototypes and data types.

-----------------------------------------------------------------------------*/

#ifndef _VSI_H_
#define _VSI_H_

//
//  If the debugging mode has not already been defined and the debugging mode
//  is desired, define the debug symbol.
//
#undef VSI_DEBUG

#include "btree.h"


//
//  Determine how the various macros will be interpreted depending on which
//  options have been enabled.
//
#ifdef VSI_DEBUG
#   ifndef LOG
#       define LOG printf
#   endif
#   ifndef HX_DUMP
#       define HX_DUMP HEX_DUMP_T
#   endif
#   ifndef SEM_DUMP
#       define SEM_DUMP dumpSemaphore
#   endif
#else
#   ifndef LOG
#       define LOG(...)
#   endif
#   ifndef HX_DUMP
#       define HX_DUMP(...)
#   endif
#   ifndef SEM_DUMP
#       define SEM_DUMP(...)
#   endif
#endif


//
//  Define the various vehicle signal domains and their type.
//
#define DOMAIN_UNKNOWN  0
#define DOMAIN_VSS      1
#define DOMAIN_CAN      2
#define DOMAIN_DBUS     3

//
//  Define the basic signal field types.
//
typedef int domain_t;
typedef int signal_t;
typedef int private_t;
typedef int group_t;

typedef offset_t name_t;        // "pointer" to the name string in SM


//
//  Declare the VSS import function.
//
int vsi_VSS_import ( const char* fileName, int domain );


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
    // TODO:  sharedMemory_t* coreHandle;

    //
    //  Define the btree indices which will be used to look up the IDs and
    //  names of signals.  Each domain/signal id and it's corresponding name
    //  will be added to both of these indices to enable lookups in either
    //  direction.
    //
    btree_t signalNameIndex;
    btree_t signalIdIndex;
    btree_t privateIdIndex;

    //
    //  Define the btree index that will be used to keep track of the groups
    //  in the system.
    //
    btree_t groupIdIndex;

}   vsi_context;

//
//  Declare the global context variable used here.
//
extern vsi_context* vsiContext;


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

    The literalData field is designed as a convenience for apps that only
    store small amounts of data (like the CAN signals for instance).  If the
    amount of storage is 8 bytes or less, the data can be stored directly in
    this literal field and the caller doesn't need to bother allocating and
    deallocating a small data buffer for this purpose.  If this field is used
    to store the actual ("literal") data then the "data" pointer should be set
    to the address of this literalData field.

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

    TODO: Add logic to group functions to lock/unlock the list!

------------------------------------------------------------------------*/
typedef struct vsi_result
 {
    domain_t        domainId;
    signal_t        signalId;
    name_t          nameOffset;
    char*           name;

    char*           data;
    unsigned long   literalData;
    unsigned long   dataLength;

    int             status;

    pthread_mutex_t lock;

}   vsi_result;


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

    @param[in] createNew - If "true", create a new empty data store.

------------------------------------------------------------------------*/
void vsi_initialize ( bool createNew );


/*!-----------------------------------------------------------------------

    v s i _ d e s t r o y

    @brief Destroy an existing VSI context.

    This function will clean up and deallocate all resources utilized by the
    VSI API and it should always be called once the VSI API is no longer
    required.  Once this function is called, the vsi_handle will no longer be
    valid and utilizing it or calling any of the VSI API functions will result
    in undefined behavior.

    @return - status - The function return status value.

------------------------------------------------------------------------*/
void vsi_destroy ( void );


/*!----------------------------------------------------------------------------

    v s i _ d e f i n e _ s i g n a l _ n a m e

    @brief Define a new VSI signal and it's name.

    This function will define a new VSI signal, creating a entry in the signal
    ID, name, and possibly the private ID indices for the signal.  The signal
    will not contain any data after this call.

    @param[in] domainId - The domain ID of the new signal
    @param[in] signalId - The signal ID of the new signal
    @param[in] privateId - The private ID of the new signal
    @param[in] name - The ASCII name of the signal

    @return 0 - Good completion, otherwise and error code.

-----------------------------------------------------------------------------*/
int vsi_define_signal ( const domain_t domainId,
                        const signal_t signalId,
                        const signal_t privateId,
                        const char*    name );

#endif  //  _VSI_H_
