/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

	@file dump.c

	This file contains a utility function to dump the contents of the shared
	memory buffers.  This is designed to be used during setup and debugging to
	enable the user to see the state of the shared memory segement after some
	changes have been made to it.

	Note that this file is the main entry point for the dump executable and
	that most of the actual work of dumping the data is done by functions in
	the utilities library.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>

#include "vsi_core_api.h"
#include "sharedMemory.h"


/*! @{ */

//
//	WARNING!!!: All references (and pointers) to data in the can message
//	buffer is performed by using the index into the array of messages.  All of
//	these references need to be relocatable references so this will work in
//	multiple processes that have their shared memory segments mapped to
//	different base addresses.
//


//
//	Define the usage message function.
//
static void usage ( const char* executable )
{
    printf ( " \n\
Usage: %s options\n\
\n\
  Option     Meaning       Type     Default   \n\
  ======  ==============  ======  =========== \n\
    -a    Dump All         bool      false    \n\
    -b    Bucket Count     int         4      \n\
    -m    Message Count    int         4      \n\
    -h    Help Message     N/A        N/A     \n\
    -?    Help Message     N/A       false    \n\
\n\n\
",
             executable );
}


/*!-----------------------------------------------------------------------

	m a i n

	@brief The main entry point for this compilation unit.

	This function will perform the dump of the shared memory segment data
	structures according to the parameters that the user has specified.

	@return  0 - This function completed without errors
    @return !0 - The error code that was encountered

------------------------------------------------------------------------*/
int main ( int argc, char* const argv[] )
{
	unsigned long messagesToDump = 4;
	unsigned long bucketsToDump  = 4;

	//
	//	The following locale settings will allow the use of the comma
	//	"thousands" separator format specifier to be used.  e.g. "10000"
	//	will print as "10,000" (using the %'u spec.).
	//
	setlocale ( LC_ALL, "");

    //
    //	Parse any command line options the user may have supplied.
    //
	char ch;

    while ( ( ch = getopt ( argc, argv, "ab:hm:?" ) ) != -1 )
    {
        switch ( ch )
        {
          case 'a':
            printf ( "Dumping all non-empty buckets.\n" );
            bucketsToDump = HASH_BUCKET_COUNT;
            messagesToDump = 999999999;

            break;

		  //
		  //	Get the requested bucket count.
		  //
		  case 'b':
		    bucketsToDump = atol ( optarg );
			if ( bucketsToDump <= 0 )
			{
				printf ( "Invalid bucket count[%lu] specified.\n",
						 bucketsToDump );
				usage ( argv[0] );
				exit (255);
			}
			break;

		  //
		  //	Get the requested message count.
		  //
		  case 'm':
		    messagesToDump = atol ( optarg );
			if ( messagesToDump <= 0 )
			{
				printf ( "Invalid message count[%lu] specified.\n",
						 messagesToDump );
				usage ( argv[0] );
				exit (255);
			}
			break;

          //
          //    Display the help message.
          //
          case 'h':
          case '?':
          default:
            usage ( argv[0] );
            exit ( 0 );
        }
    }
    //
	//	If the user supplied any arguments other than the buffer size value,
	//	they are not valid arguments so complain and quit.
    //
	argc -= optind;
    if ( argc != 0 )
    {
        printf ( "Invalid parameters[s] encountered: %s\n", argv[argc] );
        usage ( argv[0] );
        exit (255);
    }
	//
	//	Open the shared memory file.
	//
	//	Note that if the shared memory segment does not already exist, this
	//	call will create it.
	//
	vsi_core_handle handle = vsi_core_open();
	if ( handle == 0 )
	{
		printf ( "Unable to open the VSI core data store - Aborting\n" );
		exit (255);
	}
	//
	//	For each of the messages we were asked to dump...
	//
	printf ( "Beginning dump of VSI core data store[%s]...\n",
	         SHARED_MEMORY_SEGMENT_NAME );

    sharedMemory_p sharedMemory = (sharedMemory_p)handle;

	for ( unsigned int i = 0; i < bucketsToDump; i++ )
	{
		//
		//	Go dump the current hash bucket contents.
		//
		dumpHashBucket ( "", i, &sharedMemory->hashBuckets[i], messagesToDump );
	}
	//
	//	Close our shared memory segment and exit.
	//
	vsi_core_close ( handle );

	//
	//	Return a good completion code to the caller.
	//
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
