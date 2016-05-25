/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file utils.h

    This file contains the declarations of the utility functions.

-----------------------------------------------------------------------------*/

#ifndef UTILS_H
#define UTILS_H

#include "sharedMemory.h"
#include "sharedMemoryLocks.h"

/*! @{ */

//
//  Declare the dump utility functions.
//
void dumpHashBucket ( const char* leader, unsigned int bucketNumber,
					  hashBucket_p hashBucket, int maxMessages );

void dumpMessageList ( const char* leader, hashBucket_p hashBucket,
					   int maxMessages );

void dumpSemaphore ( const char* leader, semaphore_p semaphore );

//
//  Define some macros to make it easier to call the HexDump function with
//  various combinations of arguments.
//
#define HEX_DUMP( data, length )           HexDump ( data, length, "", 0 )
#define HEX_DUMP_T( data, length, title )  HexDump ( data, length, title, 0 )
#define HEX_DUMP_L( data, length, spaces ) HexDump ( data, length, "", spaces )
#define HEX_DUMP_TL                        HexDump

void HexDump ( const char *data, int length, const char *title, int leadingSpaces );

unsigned long getIntervalTime ( void );

#endif		// End of #ifndef UTILS_H

/*! @} */

// vim:filetype=h:syntax=c
