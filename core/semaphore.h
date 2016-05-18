/*!----------------------------------------------------------------------------

	@file      semaphore.h

	This file contains the declarations of the semaphore objects and member
	functions.

-----------------------------------------------------------------------------*/

#pragma once
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <pthread.h>

#include "semaphore.h"


/*! @{ */

//
//	Define the semaphore control structure.
//
typedef struct semaphore_t
{
	pthread_mutex_t mutex;
	pthread_cond_t  conditionVariable;
	int             messageCount;
	int             waiterCount;

}   semaphore_t;


//
//	Define the semaphore member functions.
//
void semaphorePost ( semaphore_t* semaphore );
void semaphoreWait ( semaphore_t* semaphore );


#endif		// End of #ifndef SEMAPHORE_H

/*! @} */

// vim:filetype=h:syntax=c
