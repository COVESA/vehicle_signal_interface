/* Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsi.h"


//
//  A few global data items to make life easier...
//
static vsi_handle handle;

// static unsigned char data;

//
//  A handy utility function to add signals to the core data store.
//
static int storeSignal ( vsi_result* result, domain_t domain_id,
                         signal_t signal_id, unsigned char dataValue )
{
    int status = 0;

    printf ( "Storing domain %u, signal %u, data %u in the core data store.\n",
             domain_id, signal_id, dataValue );

    result->domainId = domain_id;
    result->signalId = signal_id;
    result->data[0] = dataValue;
    result->dataLength = 1;

    status = vsi_fire_signal ( handle, result );

    result->status = status;
    if ( status != 0 )
    {
        printf ( "Failed to store %u, %u! Error code %d.\n", domain_id,
                 signal_id, status );
        return status;
    }
    printf ( "Successfully stored %u, %u in the core data store.\n",
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
    domain_t    domain_id;
    signal_t    signal_id;
    int         status;

    vsi_result  result;
    vsi_result* results = malloc ( sizeof(vsi_result) * 10 );

    //
    //  Call into the API to initialize the memory.
    //
    handle = vsi_initialize();
    if (!handle)
    {
        printf("Failed to allocate memory for VSI!\n");
        return -1;
    }
    vsi_context* context = handle;

    printf("Initialized the VSI API.\n");

    //
    //  Initialize the result buffer for data to go into.
    //
    //  Note that we are only going to store a single byte into the shared
    //  memory records.
    //
    result.name       = "(empty)";
    result.data       = malloc ( 32 );
    result.dataLength = sizeof(unsigned char);

    //
    //  Define some signals for the test functions.
    //
    vsi_define_signal_name ( (void*)context, 0, 1, "foo" );
    vsi_define_signal_name ( (void*)context, 0, 2, "bar" );
    vsi_define_signal_name ( (void*)context, 0, 3, "baz" );
    vsi_define_signal_name ( (void*)context, 0, 4, "gen" );
    vsi_define_signal_name ( (void*)context, 0, 5, "ivi" );

    //
    //  Fire a signal into the void to show how generating a signal works.
    //
    printf("(1) Firing signal \"bar\".\n");
    status = vsi_name_string_to_id(handle, "bar", &domain_id, &signal_id);
    if (status)
    {
        printf("Failed to find the signal ID for the signal \"bar\"! Error "
               "code %d.\n", status);
        return status;
    }
    storeSignal ( &result, domain_id, signal_id, 41 );
    printf("Successfully fired signal \"bar\".\n");

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
    printf ( "(2) Getting newest \"bar\" signal.\n" );
    result.name = "bar";
    status = vsi_get_newest_signal_by_name(handle, &result);
    if (status)
    {
        printf("Failed to get the newest signal data for \"bar\"! Error "
               "code %d.\n", status);
        return status;
    }
    printf("Successfully read the newest \"bar\" signal and got %d.\n",
           *(int*)result.data);
    //
    //  Read the oldest update from the "bar" signal.
    //
    printf ( "(3) Getting oldest \"bar\" signal.\n" );
    status = vsi_get_oldest_signal_by_name(handle, &result);
    if (status)
    {
        printf("Failed to get the oldest signal data for \"bar\"! Error "
               "code %d.\n", status);
        return status;
    }
    printf("Successfully read the oldest \"bar\" signal and got %d.\n",
           *(int*)result.data);
    //
    //  Read all of the "bar" signals from oldest to newest...
    //
    printf ( "(4) Reading the \"bar\" signals, oldest first.\n" );
    do
    {
        result.name = "bar";
        status = vsi_get_oldest_signal_by_name(handle, &result);
        if ( status != 0 )
        {
            if ( status != -ENODATA )
            {
                printf("Failed to get the oldest signal data for \"bar\"! "
                       "Error code %d.\n", status);
                return status;
            }
        }
        else
        {
            printf("Successfully read the oldest \"bar\" signal and got %d.\n",
                   *(int*)result.data);
        }
    } while (status != -ENODATA);
    printf("Completed reading the signal.\n");

    //
    //  Go through the flow of creating a group and then listening for that
    //  group to fire.
    //
    printf ( "(5) Creating a signal group.\n" );
    status = vsi_create_signal_group(handle, 10);
    if (status)
    {
        printf("Failed to create signal group 10! Error code %d.\n", status);
        return status;
    }
    printf("Created signal group 10.\n");

    printf ( "(6) Adding \"gen\" to the signal group.\n" );
    status = vsi_add_signal_to_group_by_name(handle, "gen", 10);
    if (status)
    {
        printf("Failed to add signal \"gen\" to group 10! Error code %d.\n",
               status);
        return status;
    }
    printf ( "(7) Adding \"ivi\" to the signal group.\n" );
    status = vsi_add_signal_to_group_by_name(handle, "ivi", 10);
    if (status)
    {
        printf("Failed to add signal \"ivi\" to group 10! Error code %d.\n",
               status);
        return status;
    }
    printf("Added signals \"gen\" and \"ivi\" to group 10.\n");

    //
    //  Initialize the memory for the array of vsi_result structures.
    //
    storeSignal ( &result, 0, 4, 48 );
    storeSignal ( &result, 0, 4, 49 );
    storeSignal ( &result, 0, 5, 50 );
    storeSignal ( &result, 0, 5, 51 );
    memset ( results, 0, sizeof(result) * 10 );
    for ( int i = 0; i < 10; ++i )
    {
        results[i].data = malloc ( 1 );
        results[i].dataLength = 1;
        results[i].status = -61;
    }
    //
    //  As a pit stop in this example, read the current value of the group to
    //  showcase the functionality.
    //
    printf ( "(8) Get the newest items in the group.\n" );
    status = vsi_get_newest_in_group ( handle, 10, results );
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
    printf ( "(9) Get oldest signals in the group.\n" );

    //
    //  Clear out and reinitialize out results array.
    //
    memset ( results, 0, sizeof(result) * 10 );
    for ( int i = 0; i < 10; ++i )
    {
        results[i].data = malloc ( 1 );
        results[i].dataLength = 1;
        results[i].status = -61;
    }

    status = vsi_get_oldest_in_group ( handle, 10, results );
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
    printf ( "(10) Cleaning up the group structures.\n" );
    status = vsi_delete_signal_group(handle, 10);
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
    printf ( "(11) Closing the VSI system.\n" );
    status = vsi_destroy ( handle );
    if ( status )
    {
        printf("Failed to free memory used by VSI!\n");
        return -1;
    }
    printf("Freed the VSI memory.\n");

    //
    //  Exit with a good completion code.
    //
    return 0;
}
