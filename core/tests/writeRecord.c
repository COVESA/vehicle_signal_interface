/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file writeRecord.c

    This file will write a single record into the shared memory message buffers.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <stdbool.h>

#include "vsi_core_api.h"


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
    -a    ASCII Body      string      None    \n\
    -b    Body Data       long    Same as key \n\
    -d    Domain Value     int        CAN     \n\
    -k    Key Value        int         0      \n\
    -h    Help Message     N/A        N/A     \n\
    -?    Help Message     N/A        N/A     \n\
\n\n\
",
             executable );
}


/*!-----------------------------------------------------------------------

    m a i n

    @brief The main entry point for this compilation unit.

    This function will insert a single message into the shared memory segment
    as specified by the user.

    The "domain" will default to "CAN" if not specified by the caller and the
    "key" value will default to 0.  The body data (8 bytes) will default to
    the same thing as the key if not specified.

    Note that the "body" data will be interpreted as a numeric value unless
    the user has specified the "ascii" option, in which case up to 8 bytes of
    ascii will be copied from the command line into the data portion of the
    generated message.

    If the user specifies both the numeric and ASCII forms of body data, the
    ASCII data will be used.

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
    char asciiData[9] = { 0 };
    unsigned long numericData = 0;
    unsigned long keyValue = 0;
    domain_t domainValue = DOMAIN_CAN;
    char ch;
    bool numericDataSupplied = false;

    while ( ( ch = getopt ( argc, argv, "a:b:d:k:h?" ) ) != -1 )
    {
        switch ( ch )
        {
          //
          //  Get the "copy" ASCII flag if the user specified it.
          //
          case 'a':
            strncpy ( asciiData, optarg, 8 );
            LOG ( "ASCII body data[%s] will be used.\n", asciiData );
            break;

          //
          //  Get the body data from the user.
          //
          case 'b':
            numericData = atol ( optarg );
            LOG ( "Numeric body data[%'lu] will be used.\n", numericData );
            numericDataSupplied = true;
            break;

          //
          //  Get the requested domain value.
          //
          case 'd':
            domainValue = atol ( optarg );
            LOG ( "Using domain value[%'lu]\n", domainValue );
            break;
          //
          //  Get the requested key value.
          //
          case 'k':
            keyValue = atol ( optarg );
            LOG ( "Using key value[%'lu]\n", keyValue );
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
    //  TODO: switch this to "false" for production.
    vsi_core_open ( false );

    //
    //  Go insert this message into the message pool.
    //
    //  Note that for these tests, all of the messsages in the message
    //  pool will be inserted in the "CAN" domain.
    //
    if ( strlen ( asciiData ) != 0 )
    {
        vsi_core_insert ( domainValue, keyValue, 8, &asciiData );
    }
    else
    {
        if ( ! numericDataSupplied )
        {
            numericData = keyValue;
        }
        vsi_core_insert ( domainValue, keyValue, 8, &numericData );
    }
    dumpAllSignals( 0 );

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
