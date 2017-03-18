/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

    v s i . c

    This file implements the VSI API functions.

    Note: See the vsi.h header file for a detailed description of each of the
    functions implemented here.

-----------------------------------------------------------------------------*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "vsi.h"
// #include "vsi_list.h"
// #include "btree.h"
// #include "vsi_core_api.h"
// #include "sharedMemory.h"


/*!-----------------------------------------------------------------------

    H e l p e r   M a c r o s

    The following are compiler macros that are useful in the following code.

    Helper macro to get a signal ID from a specified name. If the signal could
    not be found, an error will be returned. Rather than duplicate this logic
    across every function, define a macro for simplicity.

    This macro will trigger a return with an error code if one is returned
    from vsi_name_string_to_id. An error is detected by checking if the result
    of the call is non-zero.

------------------------------------------------------------------------*/
#define VSI_GET_VALID_ID( handle, name, domain, id )              \
{                                                                 \
    if ( ! name  )                                                \
    {                                                             \
        return -EINVAL;                                           \
    }                                                             \
    vsi_name_string_to_id ( handle, name, &domainId, &signalId ); \
}


#define VSI_LOOKUP_RESULT_NAME( handle, result )               \
{                                                              \
    if ( ! result->name  )                                     \
    {                                                          \
        return -EINVAL;                                        \
    }                                                          \
    int status = vsi_name_string_to_id ( handle, result->name, \
                                         &result->domainId,    \
                                         &result->signalId );  \
    if ( status )                                              \
    {                                                          \
        return -EINVAL;                                        \
    }                                                          \
}


/*
    Helper macro strictly for checking if input arguments are valid. This
    should be used for brevity. The expected usage is to pass in the
    conditions expected to be true and this macro will error if any of them
    are not.
*/
#define CHECK_AND_RETURN_IF_ERROR(input) \
{                                        \
    if (!(input))                        \
        return -EINVAL;                  \
}


//
//  This function will compare 2 pointers to the user defined data structures
//  above.  This function will do the comparison based on the domain and
//  signal ID.
//
//
//  This function will be called by the btree code when it has been called to
//  print out the btree nodes.  Each userData entry that is encountered in
//  that process will result in a call to this function to render the data the
//  way the user wants it to be displayed.
//
void printFunction ( char* leader, void* data )
{
    if ( data == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    vsi_id_name_definition* userDataPtr = data;

    printf ( "%sdomainId: %lu, signalId: %lu, name[%s]\n", leader,
             userDataPtr->domainId, userDataPtr->signalId, userDataPtr->name );
}


void traverseFunction ( void* dataPtr )
{
    if ( dataPtr == NULL )
    {
        printf ( "(nil)\n" );
        return;
    }
    printFunction ( "  ", dataPtr );
}


//
//  This function will compare 2 pointers to the user defined data structures
//  above.  This function will do the comparison based on the group Ids.
//
int compareGroupIds ( void* a, void* b )
{
    vsi_signal_group* userDataPtr1 = a;
    vsi_signal_group* userDataPtr2 = b;

    return ( userDataPtr1->groupId - userDataPtr2->groupId );
}


//
//  This function will be called by the btree code when it has been called to
//  print out the btree nodes.  Each userData entry that is encountered in
//  that process will result in a call to this function to render the data the
//  way the user wants it to be displayed.
//
void printGroupFunction ( char* leader, void* data )
{
    if ( data == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    vsi_signal_group* userDataPtr = data;

    //
    //  Print the list header record.
    //
    printf ( "%sgroupId: %lu, count: %d, list head: [%p]\n", leader,
             userDataPtr->groupId, userDataPtr->count, &userDataPtr->list );
    //
    //  Go get the first item in the list.
    //
    vsi_list*       listDef = &userDataPtr->list;
    vsi_list_entry* current = listDef->listHead;

    //
    //  If the list is empty, tell the caller that and quit.
    //
    if ( current == NULL )
    {
        printf ( "%s  Group has no members defined.\n", leader );
        return;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( current != NULL )
    {
        //
        //  Go print the contents of the current signal structure.
        //
        // printFunction ( leader, current->pointer );
        vsi_id_name_definition* signalData = current->pointer;
        printf ( "%s  Domain: %lu, Signal: %lu, Name: %s\n", leader,
                 signalData->domainId, signalData->signalId, signalData->name );
        //
        //  Get the next signal definition object in the list for this group.
        //
        current = current->next;
    }
    return;
}

void dumpGroups ( vsi_handle handle );


/*!-----------------------------------------------------------------------

    S t a r t u p   a n d   S h u t d o w n
    =======================================


    v s i _ i n i t i a l i z e

    Initialize the VSI API.

    If this function fails for some reason, the returned value will be NULL
    and errno will reflect the error that occurred.

    If the "createNew" argument is "true", a brand new empty data store will
    be created rather than opening an existing data store.

------------------------------------------------------------------------*/
vsi_handle vsi_initialize ( bool createNew )
{
    struct vsi_context *context;

    //
    //  Allocate our system context data structure.
    //
    context = malloc ( sizeof(struct vsi_context) );
    if ( !context )
    {
        return NULL;
    }
    //
    //  Go initialize the shared memory manager system (including the
    //  pageManager and B-tree systems).
    //
    vsi_core_open ( false );
    context->coreHandle = smControl;
    if ( ! context->coreHandle )
    {
        printf ( "Error: Unable to open the shared memory manager - Aborting!\n" );
        return NULL;
    }
    //
    //  Initialize the btree indices for the signal names and ids.
    //
    btree_create_in_place ( &context->signalNameIndex, 21, 1, 3, 0, 0, 0 );
    btree_create_in_place ( &context->signalIdIndex, 21, 2, 0, 1, 0, 0 );
    btree_create_in_place ( &context->privateIdIndex, 21, 2, 0, 2, 0, 0 );

    //
    //  Initialize the btree index for the group ids.
    //
    btree_create_in_place ( &context->groupIdIndex, 21, 1, 0, 0, 0, 0 );

    //
    //  Go import all of the VSS definitions into the system.
    //
    LOG ( "\nImporting the VSS definition file from [%s]\n", VSS_INPUT_FILE );
    (void) vsi_VSS_import ( context, VSS_INPUT_FILE );

    // LOG ( "\nImported VSS signals by ID:\n\n" );
    // btree_traverse ( &context->signalIdIndex, traverseFunction );

    // LOG ( "\nImported VSS signals by name:\n\n" );
    // btree_traverse ( &context->signalNameIndex, traverseFunction );

    //
    //  Return the context handle to the caller.
    //
    return context;
}


/*!----------------------------------------------------------------------------

    v s i _ V S S _ i m p o r t

    @brief Import a VSS file into the VSI data store.

    This function will read the specified file and import the contents of it
    into the specified VSI environment.

    @param[in] - handle - The VSI context handle
    @param[in] - fileName - The pathname to the VSS definition file to be read

    @return - Completion code - 0 = Succesful
                                Anything else is an error code

-----------------------------------------------------------------------------*/
int vsi_VSS_import ( vsi_handle handle, const char* fileName )
{
    FILE*    inputFile;
    char     name[MAX_VSS_LINE] = { 0 };
    char     line[MAX_VSS_LINE] = { 0 };
    signal_t id = 0;
    signal_t privateId = 0;
    int      tokenCount = 0;
    int      signalCount = 0;
    int      status = 0;
    bool     versionLineSeen = false;

    //
    //  If the user did not supply a valid file name, complain and quit.
    //
    if ( fileName == NULL )
    {
        //
        //  Treat the argument as the name of the VSS file to be read and open
        //  that file.
        //
        fprintf ( stderr, "ERROR: NULL VSS input file specified.\n" );
        return ENOENT;
    }
    //
    //  Attempt to open the specified file.
    //
    inputFile = fopen ( fileName, "r" );

    //
    //  If we could not open the file that was specified complain and quit.
    //
    if ( inputFile == NULL )
    {
        fprintf ( stderr, "ERROR: VSS input file[%s] could not be opened: "
                  "%d[%s]\n", fileName, errno, strerror(errno) );
        return ENOENT;
    }
    //
    //  Now read in the VSS definition file.
    //
    //  While we have not reached the end of the input file, read a line from
    //  the input file.
    //
    while ( fgets ( line, MAX_VSS_LINE, inputFile ) != NULL )
    {
        //
        //  Make sure the line is null terminated.
        //
        line[strlen(line)-1] = 0;

        //
        //  If this line begins with a '#', it is a comment line so just
        //  ignore it.
        //
        if ( line[0] == '#' )
        {
            continue;
        }
        //
        //  If it's not a comment, scan the line for the fields we need.
        //
        id        = 0;
        privateId = 0;
        tokenCount = sscanf ( line, " %s %lu %lu \n", name, &id, &privateId );

        //
        //  If this is the first line that we've seen with only 1 token on it,
        //  it must be the version number line so read it as a version string.
        //
        //  Note that if we see any more lines with only a single token, it
        //  will be an error.
        //
        if ( tokenCount == 1 && !versionLineSeen )
        {
            // TODO: Insert code here to handle the version number.
            versionLineSeen = true;
        }
        //
        //  If there are 2 tokens on the line, it should be a signal
        //  definition line.
        //
        else if ( tokenCount >= 2 )
        {
            //
            //  Go define this signal in the VSI database.
            //
            status = vsi_define_signal_name ( handle, VSS, (signal_t)id, privateId, name );

            //
            //  Increment the number of signals that we've defined.
            //
            signalCount++;

            //
            //  If the above call generated an error, let the user know about
            //  it.
            //
            if ( status != 0 )
            {
                printf ( "ERROR: Inserting data into the VSI: %d[%s]\n",
                         status, strerror(status) );
            }
        }
        //
        //  If there are more than 2 tokens on the input line, it is an error
        //  so report it to the user.
        //
        else
        {
            printf ( "ERROR: Invalid input line[%s]\n", line );
        }
    }
    //
    //  We are finished reading the input file so close it.
    //
    fclose ( inputFile );

    //
    //  Return the status code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ d e s t r o y

    Destroy the supplied VSI context->

    This function will free all resources allocated to the specified VSI
    context->  The context will not be usable after this function is called and
    accesses to it will likely result in a SEGV error.

------------------------------------------------------------------------*/
int vsi_destroy ( vsi_handle handle )
{
    vsi_context* context = handle;

    //
    //  If we still have a valid context then clean up everything.
    //
    if ( context )
    {
        //
        //  Destroy all of the btree indices that we created.
        //
        btree_destroy ( &context->signalNameIndex );
        btree_destroy ( &context->signalIdIndex );
        btree_destroy ( &context->privateIdIndex );
        btree_destroy ( &context->groupIdIndex );

        //
        //  Close the VSI core data store.
        //
        vsi_core_close();
    }
    //
    //  Finally, free the handle.
    //
    free ( handle );
    handle = NULL;

    return 0;
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G e n e r a t i o n
    =========================================

    v s i _ f i r e _ s i g n a l

    Fire a vehicle signal by its ID.

------------------------------------------------------------------------*/
int vsi_fire_signal ( vsi_handle  handle,
                      vsi_result* result )
{
    CHECK_AND_RETURN_IF_ERROR ( handle && result && result->data
                                && result->dataLength );

    vsi_core_insert ( result->domainId, result->signalId,
                      result->dataLength, &result->data );
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ f i r e _ s i g n a l _ b y _ n a m e

    Fire a vehicle signal by its name.

------------------------------------------------------------------------*/
int vsi_fire_signal_by_name ( vsi_handle  handle,
                              vsi_result* result )
{
    VSI_LOOKUP_RESULT_NAME ( handle, result );

    return vsi_fire_signal ( handle, result );
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ s i g n a l

    Fetch the oldest entry in the core database for a signal by ID.

------------------------------------------------------------------------*/
int vsi_get_oldest_signal ( vsi_handle  handle,
                            vsi_result* result )
{
    LOG ( "vsi_get_oldest_signal: %d,%d[%s]\n", result->domainId,
          result->signalId, result->name );

    CHECK_AND_RETURN_IF_ERROR ( handle && result && result->data
                                && result->dataLength );

    result->status = vsi_core_fetch ( result->domainId, result->signalId,
                                      &result->dataLength, result->data );

    LOG ( "vsi_get_oldest_signal returning: %lu\n", *(unsigned long*)result->data );

    return result->status;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ s i g n a l _ b y _ n a m e

    Fetch the oldest entry in the core database for a signal by it's name.

------------------------------------------------------------------------*/
int vsi_get_oldest_signal_by_name ( vsi_handle  handle,
                                    vsi_result* result )
{
    VSI_LOOKUP_RESULT_NAME ( handle, result );

    return vsi_get_oldest_signal ( handle, result );
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ s i g n a l

    Fetch the latest value of the specified signal by ID.

------------------------------------------------------------------------*/
int vsi_get_newest_signal ( vsi_handle  handle,
                            vsi_result* result )
{
    CHECK_AND_RETURN_IF_ERROR ( handle && result && result->data &&
                                result->dataLength );

    result->status = vsi_core_fetch_newest ( result->domainId, result->signalId,
                                             &result->dataLength, result->data );
    return result->status;
}



/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ s i g n a l _ b y _ n a m e

    Fetch the latest value of the specified signal by name.

------------------------------------------------------------------------*/
int vsi_get_newest_signal_by_name ( vsi_handle  handle,
                                    vsi_result* result )
{
    VSI_LOOKUP_RESULT_NAME ( handle, result );

    return vsi_get_newest_signal ( handle, result );
}


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l

    Flush all data for a signal by domain and ID.

------------------------------------------------------------------------*/
int vsi_flush_signal ( vsi_handle     handle,
                       const domain_t domainId,
                       const signal_t signalId )
{
    CHECK_AND_RETURN_IF_ERROR ( handle );

    return vsi_core_flush_signal ( domainId, signalId );
}


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ s i g n a l _ b y _ n a m e

    Flush all data for a signal by name.

------------------------------------------------------------------------*/
int vsi_flush_signal_by_name ( vsi_handle  handle,
                               const char* name )
{
    domain_t domainId;
    signal_t signalId;

    CHECK_AND_RETURN_IF_ERROR ( handle && name );

    VSI_GET_VALID_ID ( handle, name, &domainId, &signalId );

    return vsi_flush_signal ( handle, domainId, signalId );
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   G r o u p   F u n c t i o n s
    ===================================================

    g r o u p E x i s t s

    This is a helper function that determines if the specified group currently
    exists in the group btree database.  This function is only used by the
    internal VSI API functions.

------------------------------------------------------------------------*/
static bool groupExists ( vsi_context*  context,
                          const group_t groupId )
{
    vsi_signal_group  group;
    vsi_signal_group* temp;

    CHECK_AND_RETURN_IF_ERROR ( context && groupId );

    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    if ( temp != NULL )
    {
        return true;
    }
    return false;
}


/*!-----------------------------------------------------------------------

    v s i _ c r e a t e _ s i g n a l _ g r o u p

    @brief Create a new signal group.

------------------------------------------------------------------------*/
int vsi_create_signal_group ( vsi_handle    handle,
                              const group_t groupId )
{
    int status = 0;

    LOG ( "Creating group %lu\n", groupId );

    CHECK_AND_RETURN_IF_ERROR ( handle && groupId );

    vsi_context *context = handle;

    if ( groupExists ( context, groupId ) )
    {
        LOG ( "Error: group[%lu] already exists\n", groupId );
        return -EINVAL;
    }
    vsi_signal_group* group = malloc ( sizeof(vsi_signal_group) );

    if ( group == NULL )
    {
        printf ( "ERROR: Unable to allocate memory for group\n" );
        return -ENOMEM;
    }
    group->groupId = groupId;
    group->count   = 0;
    vsi_list_initialize ( &group->list );

    LOG ( "  inserting into btree\n" );

    status = btree_insert ( &context->groupIdIndex, group );

    LOG ( "  returning %d\n", status );

    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ d e l e t e _ s i g n a l _ g r o u p

    @brief Delete an existing signal group.

------------------------------------------------------------------------*/
int vsi_delete_signal_group ( vsi_handle    handle,
                              const group_t groupId )
{
    int status = 0;

    CHECK_AND_RETURN_IF_ERROR ( handle && groupId );

    vsi_context *context = handle;

    //
    //  Get the group object that the user specified.
    //
    vsi_signal_group  group;
    vsi_signal_group* temp;

    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the specified group does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -EINVAL;
    }
    //
    //  Go get the first item in the group list.
    //
    vsi_list*       listDef = &temp->list;
    vsi_list_entry* current = listDef->listHead;
    vsi_list_entry* next    = NULL;

    //
    //  While we have not reached the end of the signal list for this group...
    //
    while ( current != NULL )
    {
        //
        //  Save the pointer to the next list element.
        //
        next = current->next;

        //
        //  Go give this signal structure back to the system.
        //
        free ( current );

        //
        //  Move to the next signal structure in the list.
        //
        current = next;
    }
    //
    //  Go remove this signal group object from the group definition btree.
    //
    status = btree_delete ( &context->groupIdIndex, temp );

    //
    //  Give the signal group object back to the memory manager.
    //
    free ( temp );

    //
    //  Return the completion status back to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p

    @brief Add a new signal to the specified group.

    This function will add a new signal to the specified group.

------------------------------------------------------------------------*/
int vsi_add_signal_to_group ( vsi_handle     handle,
                              const domain_t domainId,
                              const signal_t signalId,
                              const group_t  groupId )
{
    int               status = 0;
    vsi_signal_group  group = { 0 };
    vsi_signal_group* temp = 0;

    CHECK_AND_RETURN_IF_ERROR ( handle && groupId );

    vsi_context *context = handle;

    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    if ( temp == NULL )
    {
        return -ENOENT;
    }
    vsi_id_name_definition* signalDef = malloc ( sizeof(vsi_id_name_definition) );

    if ( signalDef == NULL )
    {
        return -ENOMEM;
    }
    signalDef->domainId = domainId;
    signalDef->signalId = signalId;
    signalDef->name     = NULL;

    status = vsi_list_insert ( &temp->list, signalDef );

    temp->count++;

    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ a d d _ s i g n a l _ t o _ g r o u p _ b y _ n a m e

    @brief Add a new signal to the specified group by name.

    This function will add a new signal to the specified group.

------------------------------------------------------------------------*/
int vsi_add_signal_to_group_by_name ( vsi_handle    handle,
                                      const char*   name,
                                      const group_t groupId )
{
    domain_t domainId;
    signal_t signalId;

    VSI_GET_VALID_ID ( handle, name, &domainId, &signalId );

    return vsi_add_signal_to_group ( handle, domainId, signalId, groupId );
}


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p

    @brief Remove a signal from the specified group.

    This function will add a new signal to the specified group.

    TODO: This removal is awkward.  We need to search for the item we want and
    then call the remove function which will perform another search for the
    same item - Because we don't know what the "pointer" values is!  We need a
    better way to do this that is still fairly generic.

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group ( vsi_handle     handle,
                                   const domain_t domainId,
                                   const signal_t signalId,
                                   const group_t  groupId )
{
    vsi_signal_group  group;
    vsi_signal_group* temp = 0;

    CHECK_AND_RETURN_IF_ERROR ( handle && groupId );

    vsi_context *context = handle;

    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    if ( temp == NULL )
    {
        return -ENOENT;
    }
    vsi_list*               listDef = &temp->list;
    vsi_list_entry*         current = listDef->listHead;
    vsi_id_name_definition* signalDef;

    while ( current != NULL )
    {
        signalDef = current->pointer;

        if ( signalDef->domainId == domainId &&
             signalDef->signalId == signalId )
        {
            break;
        }
        current = current->next;
    }
    if ( current == NULL )
    {
        return -ENOENT;
    }
    temp->count--;
    return vsi_list_remove ( &temp->list, signalDef );
}


/*!-----------------------------------------------------------------------

    v s i _ r e m o v e _ s i g n a l _ f r o m _ g r o u p _ b y _ n a m e

    @brief Remove a signal from the specified group by name.

    This function will add a new signal to the specified group.

------------------------------------------------------------------------*/
int vsi_remove_signal_from_group_by_name ( vsi_handle    handle,
                                           const char*   name,
                                           const group_t groupId )
{
    domain_t domainId;
    signal_t signalId;

    VSI_GET_VALID_ID ( handle, name, &domainId, &signalId );

    return vsi_remove_signal_from_group ( handle, domainId, signalId, groupId );
}


/*!-----------------------------------------------------------------------

    V S I   S i g n a l   P r o c e s s i n g
    =========================================

    TODO: Note that the code in all of the functions below that operate on
    everything in a group is similar.  It would be a good idea to abstract
    these operations and use a callback function to specify the specific
    operation to be performed by a generic iterator.


    v s i _ g e t _ n e w e s t _ i n _ g r o u p

    @brief Get the newest signal available for all signals in a group.

    This function will fetch the newest (the one with the most recent
    timestamp) signal available for each member signal in the specified group.

    Note that the result pointer in this case MUST be a pointer to an array of
    result structures sufficiently large to contain one structure for each
    member of the group.  The newest signal information for each member of the
    group will be returned in one of the result structures.  Each result
    structure will also contain the completion code for the signal fetch
    operation that it represents so it is entirely possible for some signals
    to have good completion codes and some to have errors (if a signal does
    not have any data for instance).

    All of the results are retrieved in the "no wait" mode so any signals that
    do not contain any data will have an error code returned for them
    indicating that there was no data available for that signal.

    Also note that none of the signal information will be removed from the
    database during this operation.  Fetches of the "newest" signal data do
    not automatically delete that information from the database.

    TODO: Implement the "_wait" version of this function.

------------------------------------------------------------------------*/
int vsi_get_newest_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              vsi_result*   results )
{
    int                     resultIndex = 0;
    vsi_signal_group        group;
    vsi_signal_group*       temp = 0;
    vsi_id_name_definition* signalDef = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && groupId && results );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Extract the list definition structure from the signal group object.
    //
    vsi_list* listDef = &temp->list;

    //
    //  Get the a pointer to the first list entry in this group.
    //
    vsi_list_entry* current = listDef->listHead;

    //
    //  If the head pointer is NULL, the list is empty so return an error code
    //  to the caller.
    //
    if ( current == NULL )
    {
        return -ENOENT;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( current != NULL )
    {
        signalDef = current->pointer;

        //
        //  Populate the current result structure with the signal
        //  identification information.
        //
        results[resultIndex].domainId = signalDef->domainId;
        results[resultIndex].signalId = signalDef->signalId;

        //
        //  Go get the newest entry in the database for this signal.
        //
        //  Note that the return information will be populated in the result
        //  object that is passed in.
        //
        vsi_get_newest_signal ( handle, &results[resultIndex++] );

        //
        //  Get the next signal definition object in the list for this group.
        //
        current = current->next;
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ n e w e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the newest signal for every member of the specified group.

    TODO: WARNING: Unimplemented!

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
                                   vsi_result*   result )
{
    printf ( "Warning: Unimplemented function \"vsi_get_newest_in_group_wait\" "
             "called - Call ignored!\n" );

    return -ENOSYS;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p

    @brief Get the oldest signal available for all signals in a group.

    This function will fetch the oldest (the one with the least recent
    timestamp) signal available for each member signal in the specified group.

    Note that the result pointer in this case MUST be a pointer to an array of
    result structures sufficiently large to contain one structure for each
    member of the group.  The oldest signal information for each member of the
    group will be returned in one of the result structures.  Each result
    structure will also contain the completion code for the signal fetch
    operation that it represents so it is entirely possible for some signals
    to have good completion codes and some to have errors (if a signal does
    not have any data for instance).

    All of the results are retrieved in the "no wait" mode so any signals that
    do not contain any data will have an error code returned for them
    indicating that there was no data available for that signal.

    Also note that the signal information retrieved will be removed from the
    database during this operation.  Fetches of the "oldest" signal data not
    automatically delete that information from the database.

------------------------------------------------------------------------*/
int vsi_get_oldest_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              vsi_result*   results )
{
    int                     resultIndex = 0;
    vsi_signal_group        group;
    vsi_signal_group*       temp = 0;
    vsi_id_name_definition* signalDef = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && groupId && results );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Extract the list definition structure from the signal group object.
    //
    vsi_list* listDef = &temp->list;

    //
    //  Get the a pointer to the first list entry in this group.
    //
    vsi_list_entry* current = listDef->listHead;

    //
    //  If the head pointer is NULL, the list is empty so return an error code
    //  to the caller.
    //
    if ( current == NULL )
    {
        return -ENOENT;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( current != NULL )
    {
        signalDef = current->pointer;

        //
        //  Populate the current result structure with the signal
        //  identification information.
        //
        results[resultIndex].domainId = signalDef->domainId;
        results[resultIndex].signalId = signalDef->signalId;

        //
        //  Go get the oldest entry in the database for this signal.
        //
        //  Note that the return information will be populated in the result
        //  object that is passed in.
        //
        vsi_get_oldest_signal ( handle, &results[resultIndex++] );

        //
        //  Get the next signal definition object in the list for this group.
        //
        current = current->next;
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ g e t _ o l d e s t _ i n _ g r o u p _ w a i t

    @brief Retrieve the oldest signal for every member of the specified group.

    TODO: WARNING: Unimplemented!

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
int vsi_get_oldest_in_group_wait ( vsi_handle    handle,
                                   const group_t groupId,
                                   vsi_result*   result )
{
    printf ( "Warning: Unimplemented function \"vsi_get_oldest_in_group_wait\" "
             "called - Call ignored!\n" );

    return -ENOSYS;
}


/*!-----------------------------------------------------------------------

    w a i t F o r S i g n a l O n A n y

    @brief Wait for a signal to arrive.

    This is the "main" function for the threads that are created to manage
    the wait for any one of a set of signals.  We will create a thread for
    each of the signals in the set we are waiting for and the first of the
    threads that catches a signal will trigger the end of the wait and all of
    the threads in this set will be destoyed at that point.

    Note that this function does not return a value even though it claims to.
    This is because of the required function signature for callback functions
    to be used with the "pthread_create" call.

    On entry, the results object must contain the data required to make the
    call to the VSI core library to retrieve the specified signal.  That
    includes the domain ID, the signal ID, and the VSI handle.

    @param[in/out] resultsPtr - The structure to populate with the results data.
    @param[in] threadIds - The array of thread Ids that were created.
    @param[in] numberOfThreads - The number of threads that were created.

    @return None

------------------------------------------------------------------------*/
//
//  Define the data structure that is passed to the threads being created.
//
typedef struct threadData_t
{
    vsi_result* result;
    pthread_t*  threadIds;
    int         numberOfThreads;

}   threadData_t;


static void* waitForSignalOnAny ( void* resultsPtr )
{
    threadData_t* tData  = resultsPtr;
    vsi_result*   result = tData->result;
    int           status = 0;

    LOG ( "New thread %lu has started - Waiting for %lu, %lu\n",
          pthread_self() % 10000, result->domainId,
          result->signalId );
    //
    //  Go fetch the requested signal from the core data store.  Note that
    //  this function will hang on a semaphore if there is nothing to fetch.
    //  When it returns, it will have gotten the requested signal data.
    //
    status = vsi_core_fetch_wait ( result->domainId,
                                   result->signalId,
                                   (unsigned long*)result->data,
                                   &result->dataLength );
    //
    //  If the above called failed, abort this function and just quit.
    //
    if ( status != 0 )
    {
        return NULL;
    }
    LOG ( "Thread found signal, %lu, %lu status[%d], with data %u\n",
          result->domainId, result->signalId, status, result->data[0] );
    //
    //  If we get here it's because we were able to fetch the requested signal
    //  data.  At this point, we need to kill all the other threads that have
    //  not received their data yet.  Once we return, the function that
    //  spawned this thread will regain control and handle the returned data.
    //
    LOG ( "Cancelling all the other threads that were waiting...\n" );
    for ( int i = 0; i < tData->numberOfThreads; ++i )
    {
        if ( tData->threadIds[i] != pthread_self() )
        {
            LOG ( "  Cancelling thread %lu\n", tData->threadIds[i] % 10000 );
            pthread_cancel ( tData->threadIds[i] );
        }
    }
    //
    //  Exit the thread.
    //
    return NULL;
}


/*!-----------------------------------------------------------------------

    v s i _ l i s t e n _ a n y _ i n _ g r o u p

    @brief Listen for any signal in the signal list

    This function will listen for any signal in the specified group.  The
    signal list must have been previously built by adding the desired signals
    to the group before making this call.  The first signal received after
    this call is made will return it's value and the function will return to
    the caller.

    Unlike many of the other functions which return data for a group, this
    function does not expect (nor returns) an array of result structures.  It
    only returns a single result for the first signal in the group that
    contains data.

    TODO: Implement the timeout argument.
    TODO: Shouldn't this function really take a "vsi_result" argument?

    @param[in] handle - The handle to the current VSI context
    @param[out] domainId - The address of where to store the domain ID
    @param[out] signalId - The address of where to store the signal ID
    @param[out] groupId - The address of where to store the group ID
    @param[in] timeout - The optional timeout value in nanoseconds

    @return 0 if no errors occurred
              Otherwise the negative errno value will be returned.

            -EINVAL - There are no signals or groups to listen to.
            -ENOMEM - Memory could not be allocated.

------------------------------------------------------------------------*/

int vsi_listen_any_in_group ( vsi_handle    handle,
                              const group_t groupId,
                              unsigned int  timeout,
                              domain_t*     domainId,
                              signal_t*     signalId )
{
    int                     groupCount = 0;
    int                     i = 0;
    int                     status = 0;
    vsi_signal_group        group;
    vsi_signal_group*       temp = 0;
    vsi_id_name_definition* signalDef = 0;
    vsi_result*             result = 0;
    threadData_t*           tData;
    pthread_t*              threadIds = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && groupId );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -EINVAL;
    }
    //
    //  Extract the list definition structure from the signal group object.
    //
    vsi_list* listDef = &temp->list;

    //
    //  Get the a pointer to the first list entry in this group.
    //
    vsi_list_entry* current = listDef->listHead;

    //
    //  If the head pointer is NULL, the list is empty so return an error code
    //  to the caller.
    //
    if ( current == NULL )
    {
        return -ENOENT;
    }
    //
    //  Allocate the array of thread ids that we will need.
    //
    groupCount = listDef->count;
    threadIds = malloc ( groupCount * sizeof(pthread_t) );
    if ( ! threadIds )
    {
        return -ENOMEM;
    }
    //
    //  Allocate the array of result structures, one for each thread we are
    //  going to create.
    //
    vsi_result* results = malloc ( groupCount * sizeof(vsi_result) );
    if ( ! results )
    {
        return -ENOMEM;
    }
    //
    //  Allocate the array of thread data structures.
    //
    tData = malloc ( groupCount * sizeof(threadData_t) );
    if ( ! tData )
    {
        return -ENOMEM;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( current != NULL )
    {
        signalDef = current->pointer;

        LOG ( "  Creating listening thread for %lu, %lu\n",
              signalDef->domainId, signalDef->signalId );

        //
        //  Get a pointer to the current result structure.
        //
        result = &results[i];

        //
        //  Initialize the mutex for this result structure.
        //
        pthread_mutex_init ( &result->lock, NULL );

        //
        //  Specify the domain and signal IDs for the current signal in the
        //  results structure that will be passed to the new thread.
        //
        pthread_mutex_lock ( &result->lock );

        result->context = context;
        result->dataLength = 0;

        result->domainId = signalDef->domainId;
        result->signalId = signalDef->signalId;

        result->status = -ENOENT;

        pthread_mutex_unlock ( &result->lock );

        //
        //  Initialize all of the fields of the thread data structure.
        //
        tData[i].result          = results;
        tData[i].threadIds       = threadIds;
        tData[i].numberOfThreads = groupCount;

        //
        //  Go create a new thread that will wait for the specified signal to
        //  occur in the core data store.
        //
        status = pthread_create ( &threadIds[i], NULL, waitForSignalOnAny, &tData[i] );

        LOG ( "  Created thread %lu\n", threadIds[i] % 10000 );

        ++i;

        //
        //  If we could not create a new thread, return the error code to the
        //  caller.
        //
        if ( status != 0 )
        {
            printf ( "Error creating thread: %m\n" );
            return -status;
        }
        //
        //  Get the next signal definition object in the list for this group.
        //
        current = current->next;
    }
    //
    //  Wait for all of the threads to terminate.  Note that under normal
    //  conditions, one of the threads will receive the signal that it has
    //  been waiting for.  When that happens, that thread will then cancel all
    //  of the other threads that are waiting and then terminate itself.  At
    //  that point all of the threads will be dead and this loop should
    //  terminate.
    //
    LOG ( "Waiting for threads to terminate...\n" );

    for ( i = 0; i < groupCount; ++i )
    {
        LOG ( "  Waiting for thread %lu to terminate\n", threadIds[i] % 10000 );
        pthread_join ( threadIds[i], NULL );
        LOG ( "  Thread %lu has terminated\n", threadIds[i] % 10000 );

        //
        //  If this is the thread that received the signal, the status field
        //  in it's result object should be zero.
        //
        if ( results[i].status == 0 )
        {
            //
            //  Return the domain and signal id that we received.
            //
            *domainId = results[i].domainId;
            *signalId = results[i].signalId;
        }
    }
    //
    //  Clean up our temporary memory and return a good completion code to the
    //  caller.
    //
    free ( threadIds );
    free ( results );
    free ( tData );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    w a i t F o r S i g n a l O n A l l

    @brief Wait for all signals to arrive for a group.

    This is the "main" function for the threads that are created to manage the
    wait for all of the signals in a specified set.  We will create a thread
    to fetch each individual signal in the desired set and as each thread
    receives it's signal, the results for that signal will be recorded in the
    results structure assigned to it and then terminate.  When all of the
    threads have terminated, the calling function will resume and the results
    array returned to the caller.

    The caller must supply a pointer to a "vsi_result" structure that can be
    used to record the result data.  Each thread will record it's fetch signal
    data in one of these structures.

    Note that this function does not return a value even though it claims to.
    This is because of the required function signature for callback functions
    to be used with the "pthread_create" call.

    On entry, the results object must contain the data required to make the
    call to the VSI core library to retrieve the specified signal.  That
    includes the domain ID, the signal ID, and the VSI handle.

    @param[in/out] resultsPtr - The structure to populate with the results data.

    @return None

------------------------------------------------------------------------*/
static void* waitForSignalOnAll ( void* resultsPtr )
{
    threadData_t* tData  = resultsPtr;
    vsi_result*   result = tData->result;
    int           status = 0;

    LOG ( "New thread %lu has started - Waiting for %lu, %lu\n",
          pthread_self() % 10000, result->domainId, result->signalId );
    //
    //  Go fetch the requested signal from the core data store.  Note that
    //  this function will hang on a semaphore if there is nothing to fetch.
    //  When it returns, it will have gotten the requested signal data.
    //
    status = vsi_core_fetch_wait ( result->domainId,
                                   result->signalId,
                                   (unsigned long*)result->data,
                                   &result->dataLength );
    //
    //  If the above called failed, abort this function and just quit.
    //
    if ( status != 0 )
    {
        return NULL;
    }
    LOG ( "Thread found signal, %lu, %lu status[%d], with data %u\n",
          result->domainId, result->signalId, status, result->data[0] );
    //
    //  Exit the thread.
    //
    return NULL;
}


/*!-----------------------------------------------------------------------

    v s i _ l i s t e n _ a l l _ i n _ g r o u p

    @brief Listen for all signals in the specified group

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

    TODO: Implement the timeout argument.

    @param[in] handle - The handle to the current VSI context
    @param[in] groupId - The ID of the group to listen for
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
                              unsigned int  timeout )
{
    int                     groupCount = 0;
    int                     i = 0;
    int                     status = 0;
    vsi_signal_group        group;
    vsi_signal_group*       temp = 0;
    vsi_id_name_definition* signalDef = 0;
    vsi_result*             result = 0;
    threadData_t*           tData;
    pthread_t*              threadIds = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && groupId && results && resultsSize );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -EINVAL;
    }
    //
    //  Extract the list definition structure from the signal group object.
    //
    vsi_list* listDef = &temp->list;

    //
    //  If the supplied results structure array is not large enough to hold
    //  all of the results we will get, abort this call with an error code.
    //
    groupCount = listDef->count;
    if ( resultsSize < groupCount )
    {
        return -ENOMEM;
    }
    //
    //  Get the a pointer to the first list entry in this group.
    //
    vsi_list_entry* current = listDef->listHead;

    //
    //  If the head pointer is NULL, the list is empty so return an error code
    //  to the caller.
    //
    if ( current == NULL )
    {
        return -ENOENT;
    }
    //
    //  Allocate the array of thread ids that we will need.
    //
    threadIds = malloc ( groupCount * sizeof(pthread_t) );
    if ( ! threadIds )
    {
        return -ENOMEM;
    }
    //
    //  Allocate the array of thread data structures.
    //
    tData = malloc ( groupCount * sizeof(threadData_t) );
    if ( ! tData )
    {
        return -ENOMEM;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( current != NULL )
    {
        signalDef = current->pointer;

        LOG ( "  Creating listening thread for %lu, %lu\n",
              signalDef->domainId, signalDef->signalId );

        //
        //  Get a pointer to the current result structure.
        //
        result = &results[i];

        //
        //  Initialize the mutex for this result structure.
        //
        pthread_mutex_init ( &result->lock, NULL );

        //
        //  Specify the domain and signal IDs for the current signal in the
        //  results structure that will be passed to the new thread.
        //
        pthread_mutex_lock ( &result->lock );

        result->context = context;
        result->dataLength = 0;

        result->domainId = signalDef->domainId;
        result->signalId = signalDef->signalId;

        result->status = -ENOENT;

        pthread_mutex_unlock ( &result->lock );

        //
        //  Initialize all of the fields of the thread data structure.
        //
        tData[i].result          = results;
        tData[i].threadIds       = NULL;
        tData[i].numberOfThreads = 0;

        //
        //  Go create a new thread that will wait for the specified signal to
        //  occur in the core data store.
        //
        status = pthread_create ( &threadIds[i], NULL, waitForSignalOnAll, &tData[i] );

        LOG ( "  Created thread %lu\n", threadIds[i] % 10000 );

        ++i;

        //
        //  If we could not create a new thread, return the error code to the
        //  caller.
        //
        if ( status != 0 )
        {
            printf ( "Error creating thread: %m\n" );
            return -status;
        }
        //
        //  Get the next signal definition object in the list for this group.
        //
        current = current->next;
    }
    //
    //  Wait for all of the threads to terminate.  Note that under normal
    //  conditions, one of the threads will receive the signal that it has
    //  been waiting for.  When that happens, that thread will then cancel all
    //  of the other threads that are waiting and then terminate itself.  At
    //  that point all of the threads will be dead and this loop should
    //  terminate.
    //
    LOG ( "Waiting for threads to terminate...\n" );

    for ( i = 0; i < groupCount; ++i )
    {
        LOG ( "  Waiting for thread %lu to terminate\n", threadIds[i] % 10000 );
        pthread_join ( threadIds[i], NULL );
        LOG ( "  Thread %lu has terminated\n", threadIds[i] % 10000 );
    }
    //
    //  Clean up our temporary memory and return a good completion code to the
    //  caller.
    //
    free ( threadIds );
    free ( tData );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*!-----------------------------------------------------------------------

    v s i _ f l u s h _ g r o u p

    @brief Flush all of the signals available for all signals in a group.

    This function will flush all of the signals available for each member
    signal in the specified group.

    If an application is no longer interested in any of the pending signals to
    be read or simply wants to synchronize with the latest information, it may
    call this function to flush all pending messages from the queues of every
    signal in the group.

------------------------------------------------------------------------*/
int vsi_flush_group ( vsi_handle    handle,
        const group_t groupId )
{
    vsi_signal_group        group;
    vsi_signal_group*       temp = 0;
    vsi_id_name_definition* signalDef = 0;

    //
    //  Make sure the inputs are all present.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && groupId );

    //
    //  Get the context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go get the signal group structure for the specified group id.
    //
    group.groupId = groupId;

    temp = btree_search ( &context->groupIdIndex, &group );

    //
    //  If the group id specified does not exist, return an error code to the
    //  caller.
    //
    if ( temp == NULL )
    {
        return -ENOENT;
    }
    //
    //  Extract the list definition structure from the signal group object.
    //
    vsi_list* listDef = &temp->list;

    //
    //  Get the a pointer to the first list entry in this group.
    //
    vsi_list_entry* current = listDef->listHead;

    //
    //  If the head pointer is NULL, the list is empty so return an error code
    //  to the caller.
    //
    if ( current == NULL )
    {
        return -ENOENT;
    }
    //
    //  While we have not reached the end of the signal list...
    //
    while ( current != NULL )
    {
        signalDef = current->pointer;

        //
        //  Go flush all of the signal data for this signal.
        //
        vsi_flush_signal ( handle, signalDef->domainId, signalDef->signalId );

        //
        //  Get the next signal definition object in the list for this group.
        //
        current = current->next;
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


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
                            signal_t*   signalId )
{
    vsi_id_name_definition* temp;
    vsi_id_name_definition  nameIdDefinition;

    //
    //  Make sure all of the required input arguments are supplied.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && name && domainId && signalId );

    //
    //  Convert the generic handle into our context pointer.
    //
    vsi_context *context = handle;

    //
    //  Initialize the name field for the lookup operation.
    //
    nameIdDefinition.name = (char*)name;

    //
    //  Go find the record that has this name.
    //
    temp = btree_search ( &context->signalNameIndex, &nameIdDefinition );

    //
    //  If the search failed, return an error code to the caller.  The name
    //  that was specified does not exist in our indices.
    //
    if ( temp == NULL )
    {
        return -EINVAL;
    }
    //
    //  Copy the domain and signal ids from the returned data structure into
    //  the caller's return value pointers.
    //
    *domainId = temp->domainId;
    *signalId = temp->signalId;

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}

/*!-----------------------------------------------------------------------

    v s i _ s i g n a l _ i d _ t o _ s t r i n g

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
int vsi_signal_id_to_string ( vsi_handle     handle,
                              const domain_t domainId,
                              const signal_t signalId,
                              char**         name )
{
    vsi_id_name_definition* temp;
    vsi_id_name_definition  nameIdDefinition;

    //
    //  Make sure all of the required input arguments are supplied.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && name );

    //
    //  Convert the generic handle into our context pointer.
    //
    vsi_context *context = handle;

    //
    //  Initialize the id fields for the lookup operation.
    //
    nameIdDefinition.domainId = domainId;
    nameIdDefinition.signalId = signalId;

    //
    //  Go find the record that has this name.
    //
    temp = btree_search ( &context->signalIdIndex, &nameIdDefinition );

    //
    //  If the search failed, return an error code to the caller.  The id
    //  that were specified do not exist in our indices.
    //
    if ( temp == NULL )
    {
        return -EINVAL;
    }
    //
    //  Copy the domain and signal ids from the returned data structure into
    //  the caller's return value pointers.
    //
    *name = temp->name;

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


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
                             const signal_t privateId,
                             const char*    name )
{
    //
    //  Make sure all of the fields have been specified and are not null.
    //
    CHECK_AND_RETURN_IF_ERROR ( handle && name );

    LOG ( "Defining: domainId: %lu, signalId: %lu, privateId: %lu, name[%s]\n",
          domainId, signalId, privateId, name );
    //
    //  Cast the generic handle into a context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go allocate a new id/name definition data structure in memory.
    //
    vsi_id_name_definition* nameIdDefinition =
        sm_malloc ( sizeof(vsi_id_name_definition) );

    //
    //  Initialize the fields of the id/name definition structure.
    //
    nameIdDefinition->domainId = domainId;
    nameIdDefinition->signalId = signalId;
    nameIdDefinition->privateId = privateId;

    //
    //  TODO: Check for string overflow here!
    //
    nameIdDefinition->name = sm_malloc ( strlen(name) + 1 );
    strncpy ( nameIdDefinition->name, name, strlen(name) );
    nameIdDefinition->name[strlen(name)+1] = 0;

    //
    //  Go insert the new id/name definition structure into both of the btree
    //  indices defined for them.
    //
    btree_insert ( &context->signalNameIndex, nameIdDefinition );
    btree_insert ( &context->signalIdIndex,   nameIdDefinition );

    //
    //  If there is a valid private ID for this signal, add it's definition to
    //  the private index as well.
    //
    if ( privateId != 0 )
    {
        btree_insert ( &context->privateIdIndex,   nameIdDefinition );
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


static void groupTraverseFunction ( char* leader, void* dataPtr )
{
    if ( dataPtr == NULL )
    {
        printf ( "(nil)\n" );
        return;
    }
    printGroupFunction ( leader, dataPtr );
}


void dumpGroups ( vsi_handle handle )
{
    printf ( "  Dumping the group assignments...\n" );

    //
    //  Make sure the handle is not null.
    //
    if ( handle == NULL )
    {
        printf ( "Null VSI handle supplied - Aborting\n" );
        return;
    }
    //
    //  Cast the generic handle into a context pointer.
    //
    vsi_context *context = handle;

    //
    //  Go print all of the currently defined group records.
    //
    btree_traverse ( &context->groupIdIndex, groupTraverseFunction );

    return;
}
