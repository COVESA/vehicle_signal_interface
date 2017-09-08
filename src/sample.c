/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsi.h"
#include "signals.h"
#include "sharedMemory.h"
#include "vsi_core_api.h"


extern void btree_print ( btree_t* btree, printFunc printCB );

#if 0
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

    printf ( "%sUserData: domainId: %d, signalId: %d, privateId: %d, name[%s]\n",
             leader, userDataPtr->domainId, userDataPtr->signalId,
             userDataPtr->privateId, userDataPtr->name );
}
#endif


//
//  A handy utility function to add signals to the core data store.
//
static int storeSignal ( vsi_result* result, domain_t domain_id,
                         signal_t signal_id, unsigned char dataValue )
{
    int status = 0;
    unsigned long tempData = dataValue;

    printf ( "Storing domain %d, signal %d, data %u in the core data store.\n",
             domain_id, signal_id, dataValue );

    memset ( result, 0, sizeof(vsi_result) );

    if ( domain_id == 0 )
    {
        domain_id = 1;
    }

    result->domainId   = domain_id;
    result->signalId   = signal_id;
    result->data       = (char*)&tempData;
    result->dataLength = 8;

    status = vsi_insert_signal ( result );

    result->status = status;
    if ( status != 0 )
    {
        printf ( "Failed to store %d, %d! Error code %d.\n", domain_id,
                 signal_id, status );
        return status;
    }
    printf ( "Successfully stored %d, %d in the core data store.\n",
             domain_id, signal_id );

    return 0;
}


/*!-----------------------------------------------------------------------

    m a i n

    This function will test much of the functionality of the VSI API
    functions.

    @param[in] None

------------------------------------------------------------------------*/
int main()
{
    domain_t domain_id = 1;
    signal_t signal_id = 0;
    int      status    = 0;
    long     dummyData = 0;

    vsi_result  result;
    vsi_result* results = malloc ( sizeof(vsi_result) * 10 );

    //
    //  Call into the API to initialize the memory.
    //
    vsi_initialize ( true );

    printf("Initialized the VSI API.\n");

    //
    //  Initialize the result buffer for data to go into.
    //
    //  Note that we are only going to store a single byte into the shared
    //  memory records.
    //
    char* buffer = sm_malloc ( 80 );
    strcpy ( buffer, "(empty)" );

    result.nameOffset = toOffset ( buffer );
    result.data       = (char*)&dummyData;
    result.dataLength = sizeof(unsigned char);

    //
    //  Define some signals for the test functions.
    //
    vsi_define_signal ( 1, 1, 0, "foo" );
    vsi_define_signal ( 1, 2, 0, "bar" );
    vsi_define_signal ( 1, 3, 0, "baz" );
    vsi_define_signal ( 1, 4, 0, "gen" );
    vsi_define_signal ( 1, 5, 0, "ivi" );

    // btree_print ( &vsiContext->signalNameIndex, printFunction );

    //
    //  Fire a signal into the void to show how generating a signal works.
    //
    char* testSignalName = "bar";
    printf("\n(1) Firing signal \"bar\".\n");
    status = vsi_name_string_to_id(1, testSignalName, &signal_id);
    if (status)
    {
        printf("Failed to find the signal ID for the signal \"bar\"! Error "
               "code %d.\n", status);
        return status;
    }
    storeSignal ( &result, 1, signal_id, 41 );
    printf("Successfully inserted signal \"bar\".\n");

    //
    //  Fire a signal by name.
    //
    printf ( "\nStoring domain %d, signal %d, name [%s] in the core data store.\n",
             domain_id, signal_id, testSignalName );

    result.domainId   = 1;
    result.nameOffset = toOffset ( testSignalName );
    result.name       = testSignalName;
    result.data       = "insertTest";
    result.dataLength = strlen(result.data);

    status = vsi_insert_signal_by_name ( &result );

    result.status = status;
    if ( status != 0 )
    {
        printf ( "Failed to store %d,%d, [%s]! Error code %d.\n", domain_id,
	    		 signal_id, testSignalName, status );
        return status;
    }
    result.data = (char*)&dummyData;

    printf ( "Successfully stored %d,%d, [%s] in the core data store.\n",
             domain_id, signal_id, testSignalName );
    //
    //  Store another "bar" signal in the core data store with a larger data
    //  field.  We'll use this and the previous one to test the "get newest"
    //  and "get oldest" signal functions.  The oldest one has a data value of
    //  41 and the newest one has a value of 43.
    //
    storeSignal ( &result, domain_id, signal_id, 42 );
    storeSignal ( &result, domain_id, signal_id, 43 );

    //
    //  Read the latest update from the "bar" signal.
    //
    printf ( "\n(2) Getting newest \"bar\" signal.\n" );
    result.nameOffset = toOffset ( testSignalName );
    testSignalName = "bar";
    result.name = testSignalName;
    status = vsi_get_newest_signal_by_name(&result);
    if (status)
    {
        printf("Failed to get the newest signal data for \"bar\"! Error "
               "code %d.\n", status);
        return status;
    }
    printf("Successfully read the newest \"bar\" signal and got %lu.\n",
           *(unsigned long*)result.data);
    //
    //  Read the oldest update from the "bar" signal.
    //
    printf ( "\n(3) Getting oldest \"bar\" signal.\n" );
    status = vsi_get_oldest_signal_by_name(&result);
    if (status)
    {
        printf("Failed to get the oldest signal data for \"bar\"! Error "
               "code %d.\n", status);
        return status;
    }
    printf("Successfully read the oldest \"bar\" signal and got %lu.\n",
           *(unsigned long*)result.data);
    //
    //  Read all of the "bar" signals from oldest to newest...
    //
    printf ( "\n(4) Reading the \"bar\" signals, oldest first.\n" );
    do
    {
        result.nameOffset = toOffset ( testSignalName );
        status = vsi_get_oldest_signal_by_name(&result);
        if ( status != 0 )
        {
            if ( status != ENODATA )
            {
                printf("Failed to get the oldest signal data for \"bar\"! "
                       "Error code %d.\n", status);
                return status;
            }
        }
        else
        {
            printf("Successfully read the oldest \"bar\" signal and got %lu.\n",
                   *(unsigned long*)result.data);
        }
    } while (status != ENODATA);
    printf("Completed reading the signal.\n");

    //
    //  Go through the flow of creating a group and then listening for that
    //  group to insert.
    //
    printf ( "\n(5) Creating signal group 10 (should pass).\n" );
    status = vsi_create_signal_group(10);
    if (status)
    {
        printf("Failed to create signal group 10! Error code %d.\n", status);
        return status;
    }
    dumpGroups ( );

    printf("\nCreating signal group 10 again (should fail).\n");
    status = vsi_create_signal_group(10);
    if (status)
    {
        printf("Failed to create signal group 10! Error code %d.\n", status);
    }
    dumpGroups ( );

    printf("\nDeleting signal group 11 (should fail).\n");
    status = vsi_delete_signal_group (11);
    if (status)
    {
        printf("Failed to delete signal group 11! Error code %d.\n", status);
    }
    dumpGroups ( );

    printf("\nDeleting signal group 10 (should pass).\n");
    status = vsi_delete_signal_group (10);
    if (status)
    {
        printf("Failed to delete signal group 10! Error code %d.\n", status);
    }
    dumpGroups ( );

    printf("\nCreated signal group 10 again (should pass).\n");
    status = vsi_create_signal_group(10);
    if (status)
    {
        printf("Failed to create signal group 10! Error code %d.\n", status);
        return status;
    }
    dumpGroups ( );

    printf ( "\n(6) Adding \"gen\" to the signal group.\n" );
    status = vsi_add_signal_to_group_by_name ( domain_id, "gen", 10 );
    if (status)
    {
        printf("Failed to add signal \"gen\" to group 10! Error code %d.\n",
               status);
        return status;
    }
    dumpGroups ( );

    printf ( "\n(7) Adding \"ivi\" to the signal group.\n" );
    status = vsi_add_signal_to_group_by_name ( domain_id, "ivi", 10);
    if (status)
    {
        printf("Failed to add signal \"ivi\" to group 10! Error code %d.\n",
               status);
        return status;
    }
    printf("Added signals \"gen\" and \"ivi\" to group 10.\n");

    printf ( "\n(7) Adding \"Attribute.Body.BodyType\" to the signal group.\n" );
    status = vsi_add_signal_to_group_by_name ( domain_id,
                                               "Attribute.Body.BodyType", 10);
    if (status)
    {
        printf("Failed to add signal \"ivi\" to group 10! Error code %d.\n",
               status);
        return status;
    }
    printf("Added signals \"gen\", \"ivi\", and \"Attribute.Body.BodyType\" to group 10.\n");

    dumpGroups ( );

    //
    //  Initialize the memory for the array of vsi_result structures.
    //
    storeSignal ( &result, 1, 4, 48 );
    storeSignal ( &result, 1, 4, 49 );
    storeSignal ( &result, 1, 5, 50 );
    storeSignal ( &result, 1, 5, 51 );
    memset ( results, 0, sizeof(result) * 10 );
    for ( int i = 0; i < 10; ++i )
    {
        results[i].data = malloc ( 1 );
        results[i].dataLength = 1;
        results[i].status = 61;
    }
    //
    //  As a pit stop in this example, read the current value of the group to
    //  showcase the functionality.
    //
    printf ( "\n(8) Get the newest items in the group.\n" );
    status = vsi_get_newest_in_group ( 10, results );
    if ( status )
    {
        printf ( "Failed to get the newest group data for group 10! Error code %d.\n",
                 status );
        return status;
    }
    for ( int i = 0; i < 10; ++i )
    {
        if ( results[i].status == 0 )
        {
            printf ( "    Newest data for group 10[%d]: domain[%d], signal[%d],"
                     " data[%u]\n", i, results[i].domainId, results[i].signalId,
                     results[i].data[0] );
        }
    }
    printf("Successfully read all signals in group 10.\n");

    //
    //  Now go fetch the oldest signals in our group.
    //
    printf ( "\n(9) Get oldest signals in the group.\n" );

    //
    //  Clear out and reinitialize out results array.
    //
    memset ( results, 0, sizeof(result) * 10 );
    for ( int i = 0; i < 10; ++i )
    {
        results[i].data = malloc ( 1 );
        results[i].dataLength = 1;
        results[i].status = 61;
    }

    status = vsi_get_oldest_in_group ( 10, results );
    if (status)
    {
        printf("Failed to get the oldest group data for group 10! Error code %d.\n",
               status);
        return status;
    }
    for ( int i = 0; i < 10; ++i )
    {
        if ( results[i].status == 0 )
        {
            printf ( "    Oldest data for group 10[%d]: domain[%d], signal[%d],"
                     " data[%u]\n", i, results[i].domainId, results[i].signalId,
                     results[i].data[0] );
        }
    }
    printf ( "Completed reading group 10.\n" );

    //
    //  Clean up the group, since we no longer need it.
    //
    printf ( "\n(10) Cleaning up the group structures.\n" );
    status = vsi_delete_signal_group(10);
    if (status)
    {
        printf("Failed to delete signal group 10! Error code %d.\n", status);
        return status;
    }
    free ( results );
    printf("Deleted group 10.\n");

    //
    //  Finally, we close the VSI system because we are finished with it.
    //
    //  Note that we didn't bother to free all of the memory we dynamically
    //  allocated during this test but for neatness, it should normally be
    //  freed.
    //
    printf ( "\n(11) Closing the VSI system.\n" );
    vsi_destroy ( );
    if ( status )
    {
        printf("Failed to free memory used by VSI!\n");
        return -1;
    }
    printf("Freed the VSI memory.\n");

    vsi_core_close();

    //
    //  Exit with a good completion code.
    //
    return 0;
}
