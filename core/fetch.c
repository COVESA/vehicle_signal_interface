/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

	@file fetch.c

	This file will find and delete records from the shared memory message
	pool.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <stdbool.h>

#include "vsi_core_api.h"
#include "utils.h"


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
    -b    Bucket Count     int       1024     \n\
    -c    Continuous       bool      false    \n\
    -m    Message Count    int     1,000,000  \n\
    -h    Help Message     N/A        N/A     \n\
    -r    Random Write     bool    1,000,000  \n\
    -?    Help Message     N/A       false    \n\
\n\n\
",
             executable );
}


/*!-----------------------------------------------------------------------

	m a i n

	@brief  The main entry point for this compilation unit.

	This function will locate and remove messages from the shared memory
	segment as specified by the user.  By default, it will remove 1M messages
	in sequential order.

	@return  0 - This function completed without errors
    @return !0 - The error code that was encountered

------------------------------------------------------------------------*/
int main ( int argc, char* const argv[] )
{
	unsigned long   messagesToStore = 1000000;
	bool            continuousRun = false;
	bool            useRandom = false;
    vsi_core_handle handle = NULL;

	//
	//	The following locale settings will allow the use of the comma
	//	"thousands" separator format specifier to be used.  e.g. "10000"
	//	will print as "10,000" (using the %'u spec.).
	//
	setlocale ( LC_ALL, "");

    //
    //	Parse any command line options the user may have supplied.
    //
	int status;
	char ch;

    while ( ( ch = getopt ( argc, argv, "chm:r?" ) ) != -1 )
    {
        switch ( ch )
        {
		  //
		  //	Get the continuous run option flag if present.
		  //
		  case 'c':
			printf ( "Record writing will run continuously. <ctrl-c> to "
			         "quit...\n" );
		    continuousRun = true;
			break;

		  //
		  //	Get the requested buffer size argument and validate it.
		  //
		  case 'm':
		    messagesToStore = atol ( optarg );
			if ( messagesToStore <= 0 )
			{
				printf ( "Invalid buffer count[%lu] specified.\n",
						 messagesToStore );
				usage ( argv[0] );
				exit (255);
			}
			break;

		  //
		  //	Get the random insert option flag if present.
		  //
		  case 'r':
			printf ( "Record writing will be random.\n" );
		    useRandom = true;
			break;

          case 'h':
          case '?':
          default:
            usage ( argv[0] );
            exit ( 0 );
        }
    }
    //
	//	If the user supplied any arguments not parsed above, they are not
	//  valid arguments so complain and quit.
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
	handle = vsi_core_open();
	if ( handle == 0 )
	{
		printf ( "Unable to open the VSI core data store - Aborting\n" );
		exit ( 255 );
	}
	//
	//	Define the message that we will use to retrieve records from the
	//	shared memory segment.
	//
	char message[32];
	unsigned long messageKey;

	(void) memset ( &message, 0, sizeof(message) );

	//
	//	Define the performance spec variables.
	//
	struct timespec startTime;
	struct timespec stopTime;
    unsigned long   startTimeNs;
    unsigned long   stopTimeNs;
    unsigned long   diffTimeNs;
    unsigned long   totalNs = 0;
    unsigned long   totalRecords = 0;
	unsigned long   rps;

#define NS_PER_SEC ( 1000000000 )

    //
    //	Initialize the random number generator.
    //
    srand ( 1 );

	//
	//	Repeat the following at least once...
	//
	//	Note that if "continuous" mode has been selected, this loop will run
	//	forever.  The user will need to kill it manually from the command line.
	//
	//	Also note that the inclusion of the "rand" function call in the loop
	//	below has significantly slowed down the performance of the loop.
	//
	do
	{
		//
		//	For the number of iterations specified by the caller...
		//
		clock_gettime(CLOCK_REALTIME, &startTime);
		for ( unsigned int i = 0; i < messagesToStore; i++ )
		{
			//
			//	Generate a message key and use that to populate our message
			//	structure.  Note that this index may be random or sequential
			//	depending on whether the user specified the random behavior.
			//	The default is to use the sequential mode.
			//
			if ( useRandom )
			{
				messageKey = rand();
			}
			else
			{
				messageKey = i;
			}
			//
			//	Go retrieve this message from the message pool.
			//
			//	Note that for these tests, all of the messsages in the message
            //  pool will be retrieved from the "CAN" domain.
			//
            status = vsi_core_fetch ( handle, messageKey, CAN,
                                      sizeof(message), &message );
			if ( status != 0 )
			{
				printf ( "====> ERROR: Fetching message[%lu] - Error %d\n",
						 messageKey, status );
			}
		}
		clock_gettime(CLOCK_REALTIME, &stopTime);

		//
		//	Compute all of the timing metrics.
		//
		startTimeNs = startTime.tv_sec * NS_PER_SEC + startTime.tv_nsec;
		stopTimeNs  = stopTime.tv_sec  * NS_PER_SEC + stopTime.tv_nsec;
		diffTimeNs = stopTimeNs - startTimeNs;
		rps = messagesToStore / ( diffTimeNs / (double)NS_PER_SEC );
		totalNs += diffTimeNs;
		totalRecords += messagesToStore;

		//
		//	Display the amount of time it took to process this iteration of the
		//	insert loop.
		//
		printf ( "%'lu records in %'lu nsec. %'lu msec. - %'lu records/sec - Avg: %'lu\n",
				 messagesToStore,
				 stopTimeNs - startTimeNs,
				 (stopTimeNs - startTimeNs) / 1000000,
				 rps,
				 (unsigned long)( totalRecords / ( totalNs / (double)NS_PER_SEC ) )
			   );

	}   while ( continuousRun );
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
