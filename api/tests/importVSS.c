/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

	@file      importVSS.c

    This file will read a Vehicle Signal Specification (VSS) format file and
    write all of the signal definitions into the Vehicle Signal Interface
    (VSI) system.  The signal names and ids are then available for
    applications written to access the VSI data system.

-----------------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "vsi.h"


/*! @{ */


//
//  Define the maximum size of a signal name in bytes.
//
#define MAX_LINE ( 256 )

//
//  Define the data structure that we will use to associate signals with their
//  names.
//
typedef struct userData
{
    domain_t domainId;
    signal_t signalId;
    signal_t privateId;
    char*    name;

}   userData;


/*!-----------------------------------------------------------------------

    b t r e e   S u p p o r t   F u n c t i o n s

	The following functions will support various functions in the btree
    library.

------------------------------------------------------------------------*/
//
//  This function will be called by the btree code when it has been called to
//  print out the btree nodes.  Each userData entry that is encountered in
//  that process will result in a call to this function to render the data the
//  way the user wants it to be displayed.
//
static void printFunction ( char* leader, void* data )
{
    if ( data == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    userData* userDataPtr = data;

    printf ( "%sdomainId: %lu, signalId: %lu, privateId: %lu, name[%s]\n", leader,
             userDataPtr->domainId, userDataPtr->signalId,
             userDataPtr->privateId, userDataPtr->name );
}


//
//  This function will be called by the btree code when it has been called to
//  traverse the tree in userData order.  Each userData entry that is
//  encountered in the traversal process will result in a call to this
//  function to render the data the way the user wants it to be displayed.
//
static void traverseFunction ( char* leader, void* dataPtr )
{
    if ( dataPtr == NULL )
    {
        printf ( "(nil)\n" );
        return;
    }
    printFunction ( leader, dataPtr );
}


/*!-----------------------------------------------------------------------

	m a i n

	@brief The main entry point for this VSS demonstration program.

	This function will read the supplied VSS definition file and build the
    data structures to support it in memory.  It will build 2 indices on that
    data, one to fetch the name of something given it's domain and signal ID,
    and the other to fetch the domain and signal ID given the signal name.

------------------------------------------------------------------------*/
int main ( int argc, char* argv[] )
{
    char  option;
    FILE* inputFile;
    char  name[MAX_LINE] = { 0 };
    char  line[MAX_LINE] = { 0 };
    int   id = 0;
    int   privateId = 0;
    int   tokenCount = 0;
    int   signalCount = 0;
    int   status = 0;
    bool  versionLineSeen = false;
    bool  verbose = false;
    bool  veryVerbose = false;

    //
    //  While there are command line options to process...
    //
    while ( ( option = getopt ( argc, argv, "Vvh?" ) ) != -1 )
    {
        //
        //  Depending on the current option...
        //
        switch ( option )
        {
          //
          //    Display the "help" string.
          //
          case 'h':
          case '?':
            fprintf ( stderr, "Usage: %s [-vV][-h][-?] fileName\n", argv[0] );
            exit ( EXIT_FAILURE );
            break;

          //
          //    Set the "very verbose" mode that will print out details of all
          //    of the lines in the input file as they are being processed.
          //
          case 'V':
            printf ( "Very verbose output mode has been set.\n" );
            veryVerbose = true;
            verbose = true;
            break;

          //
          //    Set the normal "verbose" mode that will print out some
          //    progress information as the program runs.
          //
          case 'v':
            printf ( "Verbose output mode has been set.\n" );
            verbose = true;
            break;

          //
          //    Any other switch is an error so display the useage message and
          //    quit.
          //
          default:
            fprintf ( stderr, "Usage: %s [-vV][-h][-?] fileName\n", argv[0] );
            exit ( EXIT_FAILURE );
        }
    }
    //
    //  If there is at least one argument on the command line...
    //
    if ( optind < argc )
    {
        //
        //  Treat the argument as the name of the VSS file to be read and open
        //  that file.
        //
        inputFile = fopen ( argv[optind], "r" );
    }
    //
    //  If the user did not specify the input file, report the error and
    //  quit.
    //
    else
    {
        fprintf ( stderr, "ERROR: Missing input filename argument\n" );
        fprintf ( stderr, "Usage: %s [-vV][-h][-?] fileName\n", argv[0] );
        exit ( EXIT_FAILURE );
    }
    //
    //  If we could not open the file that was specified use "stdin" for
    //  input.
    //
    if ( inputFile == NULL )
    {
        inputFile = stdin;
        if ( verbose ) printf ( "Reading input from stdin...\n" );
    }
    //
    //  If the file opened properly, just let the user know what it is that we
    //  are reading.
    //
    else
    {
        if ( verbose ) printf ( "Reading input from [%s]...\n", argv[optind] );
    }
    //
    //  Go initialize the VSI system.
    //
    vsi_handle handle =  vsi_initialize ( true );
    if (!handle)
    {
        printf ( "Failed to initialize the VSI system!\n" );
        return -1;
    }
    if ( veryVerbose ) printf ( "Initialized the VSI API.\n" );

    //
    //  While we have not reached the end of the input file, read a line from
    //  the input file.
    //
    while ( fgets ( line, MAX_LINE, inputFile ) != NULL )
    {
        //
        //  Make sure the line is null terminated.
        //
        line[strlen(line)-1] = 0;
        if ( veryVerbose ) printf ( "=> From file[%s]\n", line );

        //
        //  If this line begins with a '#', it is a comment line so just
        //  ignore it.
        //
        if ( line[0] == '#' )
        {
            continue;
        }
        //
        //  Initialize the field variables.
        //
        id        = 0;
        privateId = 0;

        //
        //  If it's not a comment, scan the line for the fields we need.
        //
        tokenCount = sscanf ( line, " %s %d %d \n", name, &id, &privateId );

        //if ( veryVerbose )
        //{
            printf ( "==> Scaned %d tokens: name[%s], id[%d], privateId[%d]\n",
                                        tokenCount, name, id, privateId );
        //}
        //
        //  If this is the first line that we've seen with only 1 token on it,
        //  it must be the version number line so read it as a version string.
        //
        //  Note that if we see any more lines with only a single token, it
        //  will be an error.
        //
        if ( tokenCount == 1 && !versionLineSeen )
        {
            printf ( "Found VSS version number: %s\n", name );
            versionLineSeen = true;
        }
        //
        //  If there are at least 2 tokens on the line, it should be a signal
        //  definition line.
        //
        else if ( tokenCount >= 2 )
        {
            //
            //  Go define this signal in the VSI database.
            //
            status = vsi_define_signal_name ( handle, VSS, id, privateId, name );

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
            if ( veryVerbose ) printf ( "====> %s\t%d\n", name, id );
        }
    }
    //
    //  We are finished reading the input file so close it.
    //
    fclose ( inputFile );

    // print_tree ( &((vsi_context*)handle)->privateIdIndex, printFunction );

    //
    //  If we are in "verbose" mode, go dump the 3 btrees that were created
    //  while reading the input file.
    //
    if ( verbose )
    {
        printf ( "\nThe imported VSS data in ID order:...\n\n" );
        btree_traverse ( &((vsi_context*)handle)->signalIdIndex, traverseFunction );

        printf ( "\nThe imported VSS data in name order:...\n\n" );
        btree_traverse ( &((vsi_context*)handle)->signalNameIndex, traverseFunction );

        printf ( "\nThe imported VSS data in private ID order:...\n\n" );
        btree_traverse ( &((vsi_context*)handle)->privateIdIndex, traverseFunction );
    }
    //
    //  Let the user know how many signals we processed.
    //
    printf ( "%d signals were processed.\n", signalCount );

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
