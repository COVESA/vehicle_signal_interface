/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

	@file smmTests.c

	This file contains the code to perform some stand-alone tests of the
    shared memory manager module.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>

#include "vsi_core_api.h"
// #include "sharedMemoryManager.h"


/*! @{ */


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
    -b    Signal Count     int         4      \n\
    -d    Dump for domain  int         0      \n\
    -k    Dump for key     int         0      \n\
    -m    Message Count    int         4      \n\
    -h    Help Message     N/A        N/A     \n\
    -?    Help Message     N/A        N/A     \n\
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
	unsigned long listsToDump    = 4;
	// unsigned long domain         = 0;
	// unsigned long key            = 0;

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

    while ( ( ch = getopt ( argc, argv, "ab:d:hk:m:?" ) ) != -1 )
    {
        switch ( ch )
        {
          case 'a':
            printf ( "Dumping all non-empty signal lists.\n" );
            listsToDump = 4;
            messagesToDump = 999999999;

            break;

		  //
		  //	Get the requested list count.
		  //
		  case 'b':
		    listsToDump = atol ( optarg );
			if ( listsToDump <= 0 )
			{
				printf ( "Invalid list count[%lu] specified.\n", listsToDump );
				usage ( argv[0] );
				exit (255);
			}
			break;

		  //
		  //	Get the requested domain value.
		  //
		  case 'd':
		    // domain = atol ( optarg );
			break;

		  //
		  //	Get the requested key value.
		  //
		  case 'k':
		    // key = atol ( optarg );
			break;

		  //
		  //	Get the requested message count.
		  //
		  case 'm':
		    messagesToDump = atol ( optarg );
			if ( messagesToDump <= 0 )
			{
				printf ( "Invalid message count[%lu] specified.\n", messagesToDump );
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
    //  Open the shared memory file.
    //
    //  Note that if the shared memory segment does not already exist, this
    //  call will create it.
    //
    // TODO: Change to "false" for running!
    vsi_core_open ( true );

    void* mem[5] = { 0 };

    printf ( "\nAt the beginning of the tests...\n" );
    dumpSM();

    printf ( "\n  sm_malloc for 10 bytes...\n" );
    mem[0] = sm_malloc ( 10 );
    dumpSM();

    printf ( "  sm_malloc for 20 bytes...\n" );
    mem[1] = sm_malloc ( 20 );
    dumpSM();

    printf ( "  sm_malloc for 30 bytes...\n" );
    mem[2] = sm_malloc ( 30 );
    dumpSM();

    printf ( "  sm_malloc for 40 bytes...\n" );
    mem[3] = sm_malloc ( 40 );
    dumpSM();

    printf ( "  sm_malloc for 50 bytes...\n" );
    mem[4] = sm_malloc ( 50 );
    dumpSM();

    printf ( "\nAfter all of the mallocs...\n" );
    dumpSM();

    printf ( "\n  sm_free for 10 bytes...\n" );
    sm_free ( mem[0] );
    dumpSM();

    printf ( "  sm_free for 50 bytes...\n" );
    sm_free ( mem[4] );
    dumpSM();

    printf ( "  sm_free for 20 bytes...\n" );
    sm_free ( mem[1] );
    dumpSM();

    printf ( "  sm_free for 40 bytes...\n" );
    sm_free ( mem[3] );
    dumpSM();

    printf ( "  sm_free for 30 bytes...\n" );
    sm_free ( mem[2] );
    dumpSM();

    printf ( "\nAfter all of the frees...\n" );
    dumpSM();

	//
	//	Return a good completion code to the caller.
	//
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
