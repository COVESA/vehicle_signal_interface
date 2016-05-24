/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file sharedMemory.h

    This file contains the declarations of the shared memory segment constants
	and data structures and should be included in all files that need to
	operate on the shared memory segment structures.

	In this version of the shared memory segment prototype, the entire shared
	memory segment is statically defined by the definitions in this file.
	This is a temporary situation to allow the prototype code to be used for
	some preliminary testing prior to the full implementation of the shared
	memory segment prototype that will perform dynamic memory allocation
	within the share memory segment.

	The beginning of this file contains some constants that can be used to
	customize the structure of the shared memory segment including the sizes
	of the hash table and the message lists and the name of the shared memory
	segment on disk.

	WARNING!!!: All references (and pointers) to data in the shared memory
	segment are performed by using the byte index into the shared memory
	segment.  All of these references need to be relocatable references so
	this will work in multiple processes that have their shared memory
	segments mapped to different base addresses.

	Some of the constructs and functions defined here are designed to make
	porting to C++ as easy as possible so there are things here that look
	decidedly like C++ member functions for example.  There is no need for
	inheritance so that's not present (along with it's associated virtual
	function table) but I do wish that I could use function overloading in
	several places... Oh well!

-----------------------------------------------------------------------------*/

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include "sharedMemoryLocks.h"


/*! @{ */

//
//	The following is the number of hash table entries in total that are
//	defined in the hash table.  It makes the code faster if this value is a
//	power of 2 (so we can just do a simple bit mask to get the hash value for
//	each entry).
//
#define HASH_BUCKET_COUNT ( 1024 )

//
//	The following is the amount of space assigned to the actual messages for
//	an individual hash bucket.
//
#define HASH_BUCKET_DATA_SIZE  ( 1024 * 1024 )

//
//	Define the marker that we will use to identify the end of the message
//	list.
//
#define END_OF_BUCKET_DATA ( 0xfffffffffffffffful )

//
//	Define all of the possible key domain names.
//
enum domains { CAN = 1 };

//
//	Define the type to be used for all of the offset references inside the
//	shared memory segment.
//
typedef unsigned long offset_t;


/*!-----------------------------------------------------------------------

	s h a r e d M e s s a g e _ t

	@brief Define the structure of the header of each shared message.

	This message header contains all of the information required to manage a
	message in the shared memory structures.  This "shared message" structure
	is inherently variable in size, depending on how much "data" is being
	stored by the calling user.  This message header will be prepended to the
	data that the user asked us to store.

	The "key" field is the primary indexing value for this message.  It is
	defined here as an unsigned 64 bit numberic value.  Keys must be unique
	within a domain for all messages that represent the same type of data.

	The "domain" is a small integer used to identify the domain of a key.  The
	domain is intended as an identifier that will be the same for all messages
	that belong to that domain.  For instance, all messages coming form the
	CAN bus will be in the CAN domain even though there may be 1500 or more
	distinct types of CAN messages.  Each distinct type within the CAN domain
	is identified by a unique "key" value.

	The "messageSize" field is the number of bytes of data that is being
	stored in the "data" field of this structure.  This value does not include
	this message header size, just the amount of data stored in this record.

	The "nextMessageOffset" is the offset within this hash bucket where the
	"next" message in a message list is stored.  Traversing down the list from
	the hash bucket "head" field will allow all of the messages in this hash
	bucket to be found.  The end of the message list is marked with a special
	marker defined above.

	The "waitCount" is used when a new message is being inserted into the
	message list.  The number of processes waiting for this message will be
	copied into this field (from the semaphore count field) when the message
	is inserted.  All of the waiting processes will then be released (via a
	"broadcast" condition variable call) and each of them will then be able to
	retrieve the new message when they become scheduled to run.  When one of
	those processes fetches the new message, the message will not be removed
	from the message list, but the "waitCount" will be decremented by one each
	time the message is copied for a process.  Only after the "waitCount" has
	been decremented to zero will the message actually be removed from the
	message list, thereby ensuring that all processes that were waiting for
	this message have gotten a copy of the message before the message is
	removed from the message list.

	The "data" field is where the actual data that the user has asked us to
	store will be copied.  This is an array of bytes whose size depends on the
	"messageSize" that the caller has specified.

------------------------------------------------------------------------*/
typedef struct sharedMessage_t
{
	offset_t     key;
	offset_t     messageSize;
	offset_t     nextMessageOffset;
	enum domains domain;
	char         data[0];

}   sharedMessage_t;

//
//	Define the size of the shared message header structure.  This value needs
//	to be taken into account when the total size of a message is computed.
//	The "messageSize" field in this header is the size of the "data" field at
//	the end of the structure so to get the total size of a structure, we must
//	use:
//
//    totalMessageSize = messagePtr->messageSize + HEADER_SIZE;
//
#define HEADER_SIZE ( sizeof(struct sharedMessage_t) )


/*!-----------------------------------------------------------------------

	h a s h B u c k e t _ t

	@brief Define the structure of each hash table bucket.

	This function will define the structure of each hash table bucket in the
	hash table.

------------------------------------------------------------------------*/
typedef struct hashBucket_t
{
	//
	//	Define the variables that will be used to keep track of the linked
	//	list data within each hash bucket.
	//
	//	Note: The "head" is the offset to the first available message in the
	//	current list and the "tail" is the offset to the last available
	//	message in the list (i.e. NOT the next available message slot).  This
	//	is required because we need to be able to update the "next" pointer in
	//	the last message whenever we append a new message to the list and we
	//	can't back up in the list (it's a singly linked list).
	//
	offset_t      head;
	offset_t      tail;
	unsigned long currentMessageCount;
	unsigned long generationNumber;
	unsigned long messageSequenceNumber;
	unsigned long totalMessageSize;

	//
    //	Define the mutex that will be used to lock this shared memory
	//	segment when it is being updated.
    //
	pthread_mutex_t lock;

	//
	//	Define the semaphore that will be used to manage the processes waiting
	//	for messages on the message queue.  Each message that is received will
	//	increment the semaphore and each "wait" that is executed will
	//	decrement the semaphore.
	//
    semaphore_t semaphore;

	//
	//	Define the actual data storage area for this hash bucket.
	//
	//	The messages that are stored here are variable size.  Each message
	//	will contain the size of the message and the offset in the data area
	//	to where the next message begins.  Messages are inserted into the
	//	list at the tail pointer and removed from the head pointer resulting
	//	in a FIFO queue behavior.
	//
	//	When messages being inserted will exceed the size of the hash bucket,
	//	we will wrap back to the beginning of the list and begin inserting
	//	from there again.  If the list becomes full, we will begin overwriting
	//	older messages with the new ones so the oldest messages will be lost.
	//
	unsigned char data[HASH_BUCKET_DATA_SIZE];

}	hashBucket_t;


/*!-----------------------------------------------------------------------

	H a s h   b u c k e t   m e m b e r   f u n c t i o n s

------------------------------------------------------------------------*/
//
//	Return the address of a message from the specified hash bucket.
//
//	This function will return the actual pointer into the shared memory
//	segment that corresponds to the location of the "offset" within the
//	specified hash bucket.  These pointers are necessary when we want to
//	actually read and write data into the messages being stored.
//
inline static sharedMessage_t* hb_getAddress ( hashBucket_t* hashBucket,
											   offset_t offset )
{
	return (sharedMessage_t*) ( &hashBucket->data[offset] );
}


//
//	Return the offset of the first message in the message list of this hash
//	bucket.
//
inline static offset_t hb_getHead ( hashBucket_t* hashBucket )
{
	return hashBucket->head;
}


//
//	Return the offset of the last message in the message list of this hash
//	bucket.
//
inline static offset_t hb_getTail ( hashBucket_t* hashBucket )
{
	return hashBucket->tail;
}


/*!-----------------------------------------------------------------------

	s h a r e d M e m o r y _ t

	@brief	Define the structure of the entire shared memory segment.

	This data structure defines the entire shared memory segment.

------------------------------------------------------------------------*/
typedef struct sharedMemory_t
{
	//
	//	Define the actual hash table and storage area.
	//
	hashBucket_t hashBuckets[HASH_BUCKET_COUNT];

	unsigned long globalTime;       // Debugging Only (for delta times)

}	sharedMemory_t;


//
//	Define the global constants and variables.
//
#define TOTAL_SHARED_MEMORY_SIZE ( sizeof(sharedMemory_t) )
#define SHARED_MEMORY_SEGMENT_NAME "/var/run/shm/vsiSharedMemorySegment"


/*!-----------------------------------------------------------------------

    S h a r e d   M e m o r y   F u n c t i o n s

------------------------------------------------------------------------*/
//
//	Return the hash bucket that corresponds to the specified key value.
//
//	This is the implementation of the hashing function that operates on the
//	supplied key value and return an index into the array of hash buckets.
//
inline static unsigned long sm_getHash ( unsigned long key )
{
	return key % HASH_BUCKET_COUNT;
}


//
//	Return the address of a hash bucket from the specified shared memory
//	segment at the specified index.
//
inline static hashBucket_t* sm_getBucketAddress ( sharedMemory_t* sharedMemory,
											      unsigned int index )
{
	return (hashBucket_t*)( &sharedMemory->hashBuckets[index] );
}


//
//	Initialize the shared memory segment.
//
//  This function will take a newly created shared memory segment file
//  descriptor and size, map the file into memory, and initialize all of the
//  fields and variables in the structure.
//
sharedMemory_t* sm_initialize ( int fd, size_t sharedMemorySegmentSize );


#endif	// End of #ifndef SHARED_MEMORY_H

/*! @} */

// vim:filetype=h:syntax=c
