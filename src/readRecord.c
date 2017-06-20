/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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

#include "vsi.h"
#include "vsi_core_api.h"
#include "signals.h"


/*! @{ */

//
//  Define the usage message function.
//
static void usage ( const char* executable )
{
    printf ( " \n\
Usage: %s options\n\
\n\
  Option     Meaning       Type     Default   \n\
  ======  ==============  ======  =========== \n\
    -d    Domain Value     int         1      \n\
    -h    Help Message     N/A        N/A     \n\
    -s    Signal Value     int        N/A     \n\
    -o    Find Oldest      N/A       true     \n\
    -?    Help Message     N/A        N/A     \n\
\n\n\
",
    executable );
}


/*!-----------------------------------------------------------------------

    m a i n

    @brief The main entry point for this compilation unit.

    This function will read and possibly remove a single message from the
    shared memory segment as specified by the user.

    The "domain" will default to "CAN" if not specified by the caller.

    Note that the data value will be read as 8 bytes of data and interpreted
    as an unsigned long numeric value.

    @return  0 - This function completed without errors
    @return !0 - The error code that was encountered

------------------------------------------------------------------------*/
int main ( int argc, char* const argv[] )
{
    //
    //  The following locale settings will allow the use of the comma
    //  "thousands" separator format specifier to be used.  e.g. "10000"
    //  will print as "10,000" (using the %'u spec.).
    //
    setlocale ( LC_ALL, "");

    //
    //  Parse any command line options the user may have supplied.
    //
    signal_t      signal = 0;
    domain_t      domain = 1;
    unsigned long data = 0;
    unsigned long dataSize = sizeof(data);
    int           status = 0;
    char          ch;
    bool          getOldest = true;

    while ( ( ch = getopt ( argc, argv, "d:hos:?" ) ) != -1 )
    {
        switch ( ch )
        {
          //
          //  Get the requested domain value.
          //
          case 'd':
            domain = atol ( optarg );
            LOG ( "Using domain value[%'d]\n", domain );
            break;

          //
          //  Get the requested key value.
          //
          case 's':
            signal = atol ( optarg );
            LOG ( "Using signal value[%'d]\n", signal );
            break;

          //
          //  Get the "newest" message flag.
          //
          case 'o':
            LOG ( "Fetching the oldest signal\n" );
            getOldest = false;
            break;

          //
          //  Display the help message.
          //
          case 'h':
          case '?':
          default:
            usage ( argv[0] );
            exit ( 0 );
        }
    }
    //
    //  If the user supplied any arguments not parsed above, they are not
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
    //  Open the shared memory file.
    //
    //  Note that if the shared memory segment does not already exist, this
    //  call will create it.
    //
    vsi_initialize ( false );

    //
    //  Go read this message from the message pool.
    //
    LOG ( "  Fetching domain: %'d\n           signal...: %'d\n", domain, signal );

    if ( getOldest )
    {
        status = vsi_core_fetch_wait ( domain, signal, &dataSize, &data );
    }
    else
    {
        status = vsi_core_fetch_newest ( domain, signal, &dataSize, &data );
    }
    if ( status == 0 )
    {
#ifdef VSI_DEBUG
        HX_DUMP ( data, dataSize, "  Returned Value: " );
#endif
        printf ( "  Returned Value: %lu\n", data );
    }
    else
    {
        printf ( "----> Error %d[%s] returned\n", status, strerror(status) );
    }
    //
    //  Close our shared memory segment and exit.
    //
    vsi_core_close();

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
