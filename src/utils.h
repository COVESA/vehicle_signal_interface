/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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
// #include "sharedMemoryLocks.h"

/*! @{ */

//
//  Declare the dump utility functions.
//
void dumpSemaphore ( semaphore_p semaphore );

#ifdef LOG

unsigned long getIntervalTime ( void );

#endif


#endif      // End of #ifndef UTILS_H

/*! @} */

// vim:filetype=h:syntax=c
