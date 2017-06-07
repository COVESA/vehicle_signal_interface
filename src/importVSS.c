/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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
#include "signals.h"
#include "vsi_core_api.h"


/*! @{ */


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
    domain_t domain = DOMAIN_VSS;

    //
    //  If the user did not specify the filename argument, complain and quit.
    //
    if ( argc < 2 )
    {
        fprintf ( stderr, "ERROR: Missing input filename argument\n" );
        fprintf ( stderr, "Usage: %s fileName domain\n", argv[0] );
        exit ( EXIT_FAILURE );
    }
    //
    //  Go initialize the VSI system.
    //
    vsi_initialize ( true );

    //
    //  Go input all of the VSS data.
    //
    if ( argc < 3 )
    {
        fprintf ( stderr, "Warning: No domain specified, using [%d]\n", domain );
    }
    else
    {
        domain = atoi ( argv[2] );
    }
    (void)vsi_VSS_import ( argv[1], domain );

    //
    //  Go close the VSI system.
    //
    vsi_core_close();

    //
    //  Return a good completion code to the caller.
    //
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
