/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

	@file readRecord.c

	This file will read a single record from the shared memory message buffers.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#include "vsi_core_api.h"


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
	-d    Domain Value     int        CAN     \n\
	-k    Key Value        int         0	  \n\
	-n    Find Newest      N/A       false    \n\
    -h    Help Message     N/A        N/A     \n\
    -?    Help Message     N/A        N/A     \n\
\n\n\
",
             executable );
}


/*!-----------------------------------------------------------------------

	m a i n

	@brief The main entry point for this compilation unit.

	This function will read and remove a single message from the shared memory segment
	as specified by the user.

	The "domain" will default to "CAN" if not specified by the caller and the
	"key" value will default to 0.

	Note that the "body" data will be read as 8 bytes of data and interpreted
	as a numeric value and as an ASCII string on output.

	@return  0 - This function completed without errors
	@return !0 - The error code that was encountered

------------------------------------------------------------------------*/
int main ( int argc, char* const argv[] )
{
	//
	//	The following locale settings will allow the use of the comma
	//	"thousands" separator format specifier to be used.  e.g. "10000"
	//	will print as "10,000" (using the %'u spec.).
	//
	setlocale ( LC_ALL, "");

    //
    //	Parse any command line options the user may have supplied.
    //
	char asciiData[9] = { 0 };		// We only copy 8 bytes so it is always
									// null terminated
	unsigned long keyValue = 0;
	domains domainValue    = CAN;
	int status             = 0;
	char ch                = 0;
    bool getNewest         = false;

    while ( ( ch = getopt ( argc, argv, "d:k:nh?" ) ) != -1 )
    {
        switch ( ch )
        {
		  //
		  //	Get the requested domain value.
		  //
		  case 'd':
		    domainValue = atol ( optarg );
			LOG ( "Using domain value[%'u]\n", domainValue );
			break;
		  //
		  //	Get the requested key value.
		  //
		  case 'k':
		    keyValue = atol ( optarg );
			LOG ( "Using key value[%'lu]\n", keyValue );
			break;
		  //
		  //	Get the "newest" message flag.
		  //
		  case 'n':
			LOG ( "Using key value[%'lu]\n", keyValue );
            getNewest = true;
			break;
		  //
		  //	Display the help message.
		  //
          case 'h':
          case '?':
          default:
            usage ( argv[0] );
            exit ( 0 );
        }
    }
    //
	//	If the user supplied any arguments not parsed above, they are not
	//	valid arguments so complain and quit.
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
		exit ( 255 );
	}
	//
	//	Go read this message from the message pool.
	//
	LOG ( "  domain: %'u\n  key...: %'lu\n", domainValue, keyValue );

    unsigned long dataSize = sizeof(asciiData) - 1;
    if ( getNewest )
    {
        status = vsi_core_fetch_newest ( handle, domainValue, keyValue, &dataSize,
                                         asciiData );
    }
    else
    {
        status = vsi_core_fetch_wait ( handle, domainValue, keyValue, &dataSize,
                                       asciiData );
    }

	if ( status == 0 )
	{
#ifdef VSI_DEBUG
        unsigned long numericData = atol ( asciiData );
		LOG ( "  value.: %'lu[%s]\n", numericData, asciiData );
#endif
	}
	else
	{
		printf ( "----> Error %d[%s] returned\n", status, strerror(status) );
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
