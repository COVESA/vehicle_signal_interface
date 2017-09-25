/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "vsi.h"
#include "signals.h"
#include "vsi_core_api.h"


//
//  Define the global VSI "context" structure.
//
vsi_context* vsiContext = 0;


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
void vsi_initialize ( bool createNew )
{
    btree_key_def* keyDef = 0;

    //
    //  Go initialize the shared memory manager system (including the
    //  pageManager and B-tree systems).
    //
    vsi_core_open ( createNew );

    //
    //  S i g n a l   N a m e   I n d e x
    //
    //  The key here is a domain and a null terminated string.  That means the
    //  data structure will have a pointer (actually an offset_t value) stored
    //  in it as the key in addition to the domain ID value.
    //
    //  If the btree has not yet been initialized...
    //
    //  Note that we can tell if this has been initialized by comparing two of
    //  the fields that are the same size.  If they are equal then the btree
    //  has not been initialized because the whole data structure has been
    //  filled with the same pattern.  We choose to compare the minimum degree
    //  with the minimum value as those should always differ by 1.
    //
    if ( vsiContext->signalNameIndex.minDegree - vsiContext->signalNameIndex.min == 0 )
    {
        //
        //  Go allocate space in the shared memory segment for the name key
        //  definition data structures.
        //
        keyDef = sm_malloc ( KEY_DEF_SIZE(2) );

        if ( keyDef == NULL )
        {
            printf ( "Error: Unable to allocate memory for the key definitions "
                     "- Aborting!\n" );
            return;
        }
        //
        //  Populate the name index key definition structures.
        //
        //  The name index only has 1 key field which is a pointer (offset_t
        //  actually) to the name string so initialize the key definition
        //  fields accordingly.
        //
        keyDef->fieldCount = 2;

        keyDef->btreeFields[0].type   = ft_int;
        keyDef->btreeFields[0].offset = offsetof ( signal_list, domainId );
        keyDef->btreeFields[0].size   = 1;

        keyDef->btreeFields[1].type   = ft_string;
        keyDef->btreeFields[1].offset = offsetof ( signal_list, name );
        keyDef->btreeFields[1].size   = 1;

        //
        //  Go initialize the signal name btree index.
        //
        btree_create_in_place ( &vsiContext->signalNameIndex, 21, keyDef );
    }
    //
    //  S i g n a l   I D   I n d e x
    //
    //  If the btree has not yet been initialized...
    //
    if ( vsiContext->signalIdIndex.minDegree - vsiContext->signalIdIndex.min == 0 )
    {
        //
        //  Go allocate space in the shared memory segment for the signal ID
        //  key definition data structures.
        //
        keyDef = sm_malloc ( KEY_DEF_SIZE(2) );

        if ( keyDef == NULL )
        {
            printf ( "Error: Unable to allocate memory for the key definitions "
                     "- Aborting!\n" );
            return;
        }
        //
        //  Populate the signal ID index key definition structures.
        //
        //  The signal ID index only has 1 key field which is an int type so
        //  initialize the key definition fields accordingly.
        //
        keyDef->fieldCount = 2;

        keyDef->btreeFields[0].type   = ft_int;
        keyDef->btreeFields[0].offset = offsetof ( signal_list, domainId );
        keyDef->btreeFields[0].size   = 1;

        keyDef->btreeFields[1].type   = ft_int;
        keyDef->btreeFields[1].offset = offsetof ( signal_list, signalId );
        keyDef->btreeFields[1].size   = 1;

        //
        //  Go initialize the signal ID btree index.
        //
        btree_create_in_place ( &vsiContext->signalIdIndex, 21, keyDef );
    }
    //
    //  P r i v a t e   I D   I n d e x
    //
    //  If the btree has not yet been initialized...
    //
    if ( vsiContext->privateIdIndex.minDegree - vsiContext->privateIdIndex.min == 0 )
    {
        //
        //  Go allocate space in the shared memory segment for the private ID
        //  key definition data structures.
        //
        keyDef = sm_malloc ( KEY_DEF_SIZE(2) );

        if ( keyDef == NULL )
        {
            printf ( "Error: Unable to allocate memory for the key definitions "
                     "- Aborting!\n" );
            return;
        }
        //
        //  Populate the private ID index key definition structures.
        //
        //  The private ID index has 2 key fields which are both an int type
        //  so initialize the key definition fields accordingly.
        //
        keyDef->fieldCount = 2;

        keyDef->btreeFields[0].type   = ft_int;
        keyDef->btreeFields[0].offset = offsetof ( signal_list, domainId );
        keyDef->btreeFields[0].size   = 1;

        keyDef->btreeFields[1].type   = ft_int;
        keyDef->btreeFields[1].offset = offsetof ( signal_list, privateId );
        keyDef->btreeFields[1].size   = 1;

        //
        //  Go initialize the private ID btree index.
        //
        btree_create_in_place ( &vsiContext->privateIdIndex, 21, keyDef );
    }
    //
    //  G r o u p   I D   I n d e x
    //
    //  The group ID key here is a single integer field.
    //
    //  If the btree has not yet been initialized...
    //
    if ( vsiContext->groupIdIndex.minDegree - vsiContext->groupIdIndex.min == 0 )
    {
        //
        //  Go allocate space in the shared memory segment for the group ID
        //  key definition data structures.
        //
        keyDef = sm_malloc ( KEY_DEF_SIZE(1) );

        if ( keyDef == NULL )
        {
            printf ( "Error: Unable to allocate memory for the key definitions "
                     "- Aborting!\n" );
            return;
        }
        //
        //  Populate the group ID index key definition structures.
        //
        //  The group ID index only has 1 key field which is an int type so
        //  initialize the key definition fields accordingly.
        //
        keyDef->fieldCount = 1;

        keyDef->btreeFields[0].type   = ft_int;
        keyDef->btreeFields[0].offset = offsetof ( vsi_signal_group, groupId );
        keyDef->btreeFields[0].size   = 1;

        //
        //  Go initialize the btree index for the group ids.
        //
        btree_create_in_place ( &vsiContext->groupIdIndex, 21, keyDef );
    }
    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    v s i _ V S S _ i m p o r t

    @brief Import a VSS file into the VSI data store.

    This function will read the specified file and import the contents of it
    into the specified VSI environment.

    @param[in] - fileName - The pathname to the VSS definition file to be read

    @return - Completion code - 0 = Succesful
                                Anything else is an error code

-----------------------------------------------------------------------------*/
#define MAX_VSS_LINE ( 256 )

int vsi_VSS_import ( const char* fileName, int domain )
{
    FILE*    inputFile;
    char     name[MAX_VSS_LINE] = { 0 };
    char     line[MAX_VSS_LINE] = { 0 };
    signal_t id = 0;
    signal_t privateId = 0;
    int      tokenCount = 0;
    int      signalCount = 0;
    int      lineCount = 0;
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
        //  Increment the line count in the source file.
        //
        lineCount++;

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
        memset ( name, 0, MAX_VSS_LINE );

        tokenCount = sscanf ( line, " %s %d %d \n", name, &id, &privateId );

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
            printf ( "Found VSS version number: %s\n", name );
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
            status = vsi_define_signal ( domain, (signal_t)id,
                                         (signal_t)privateId, name );
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
            printf ( "Importing signal %d at line %d: %u - %s\n",
                     signalCount, lineCount, id, name );

#ifdef VSI_DEBUG
            btree_print ( &vsiContext->signalNameIndex, NULL );
#endif
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
    printf ( "%d signal names defined for domain %d\n", signalCount, domain );
    fclose ( inputFile );

    //
    //  Go dump the resulting "name" Btree.
    //
    btree_print ( &vsiContext->signalNameIndex, NULL );

    //
    //  Return the status code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    v s i _ d e s t r o y

    Destroy the supplied VSI context

    This function will free all resources allocated to the specified VSI
    context.  The context will not be usable after this function is called and
    accesses to it will likely result in a SEGV error.

------------------------------------------------------------------------*/
void vsi_destroy ( void )
{
    //
    //  If we still have a valid vsiContext then clean up everything.
    //
    if ( vsiContext )
    {
        //
        //  Destroy all of the btree indices that we created.
        //
        btree_destroy ( &vsiContext->signalNameIndex );
        btree_destroy ( &vsiContext->signalIdIndex );
        btree_destroy ( &vsiContext->privateIdIndex );
        btree_destroy ( &vsiContext->groupIdIndex );

        //
        //  Close the VSI core data store.
        //
        vsi_core_close();
    }
    //
    //  Finally, free the handle.
    //
    free ( vsiContext );
    vsiContext = NULL;

    return;
}

