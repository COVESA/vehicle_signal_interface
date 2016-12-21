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

	The beginning of this file contains some constants that can be used to
	customize various characteristics of the data store.

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

//
//  Include the system header files we need.
//
#include <pthread.h>
#include <stdbool.h>

//
//  Define any of the conditional compilation flags that we want to enable.
//
#undef VSI_DEBUG
#undef DUMP_SEMAPHORE

//
//  Include the local header files we need.
//
#include "sharedMemoryLocks.h"
#include "btree.h"


//
//  Determine how the various macros will be intrepreted depending on which
//  options have been enabled.
//
#ifdef VSI_DEBUG
#   define LOG      printf
#   define HX_DUMP  HEX_DUMP_T
#else
#   define LOG(...)
#   define HX_DUMP(...)
#endif

#ifdef DUMP_SEMAPHORE
#   define SEM_DUMP dumpSemaphore
#else
#   define SEM_DUMP(...)
#endif


//
//	Define the global constants and variables.
//
//  Note that the shared memory size will always be allocated (by the kernel)
//  in units of whole memory pages.  Most modern system have 4K byte memory
//  pages so it is most efficient to allocate the memory in units of 4K bytes.
//
//  We will allocate 2 separate shared memory spaces.  The first is used for
//  all of the user defined data including any B-tree structures defined by
//  the users.  The initial allocation for this segment is 2GB and will be
//  referred to as the "user space".
//
//  The second shared memory space is used by the shared memory manager to
//  implement the "sm_malloc" and "sm_free" functions to keep track of freed
//  memory in the shared memory segment.  The initial allocation for this
//  segment is 2 MB and will be referred to as the "system space".  Note that
//  this does not refer to the kernel space or library space used the the
//  Linux "system", it is just the VSI "system" code.
//
//  Both segments can be dynamically resized as needed with minimal cost.
//
#define SHARED_MEMORY_SEGMENT_NAME "/var/run/shm/vsiUserDataStore"
#define INITIAL_SHARED_MEMORY_SIZE ( 2 * 1024L * 1024L )

#define SYS_SHARED_MEMORY_SEGMENT_NAME "/var/run/shm/vsiSysDataStore"
#define SYS_INITIAL_SHARED_MEMORY_SIZE ( 1 * 1024L * 1024L )

//
//  Define the maximum number of records in each node of the 2 shared memory
//  memory management B-trees in the system space.
//
//  Note: The record count should be an odd number.  If an even number is
//  specified, the btree_initialize function will increment it to be odd.
//
#define SYS_RECORD_COUNT     ( 21 )

#define SYS_SIZE_KEY_COUNT   (  2 )
#define SYS_SIZE_KEY1        (  1 )
#define SYS_SIZE_KEY2        (  2 )

#define SYS_OFFSET_KEY_COUNT (  1 )
#define SYS_OFFSET_KEY1      (  2 )
#define SYS_OFFSET_KEY2      (  0 )

//
//  Define the maximum number of records in each node of the "signals" B-tree
//  structure defined in the user space.
//
#define SM_SIGNALS_RECORD_COUNT           ( 41 )
#define SM_SIGNALS_KEY_COUNT              (  2 )
#define SM_SIGNALS_KEY1                   (  0 )
#define SM_SIGNALS_KEY2                   (  1 )


/*! @{ */


//
//  Declare the address of the global shared memory control structures.
//
//  TODO: I'd really like to get rid of this but there are some areas of the
//  code that require it like the sm_malloc and sm_free functions.
//
extern struct sharedMemory_t* smControl;
extern struct sysMemory_t*    sysControl;


//
//  Define the various vehicle signal domain_t.
//
#define DOMAIN_VSS  1
#define DOMAIN_CAN  2
#define DOMAIN_DBUS 3


//
//	Define the type to be used for all of the offset references inside the
//	shared memory segment.
//
typedef unsigned long offset_t;
typedef unsigned long domain_t;
typedef unsigned long vsiKey_t;


/*!-----------------------------------------------------------------------

    m e m o r y C h u n k _ t

	@brief Define the shared memory memory region control structure.

    Define the data structure that will be used to keep track of chunks of
    memory in the B-trees.  This data structure is prepended to each memory
    allocation to allow us to manage the individual pieces when they are
    subsequently freed.

    The "marker" is a known bit pattern that we will use to identify valid
    memoryChunk_t blocks and by defining 2 different patterns, we can also
    identify "allocated" vs. "free" memory blocks.

    The "segmentSize" is the size of this block of memory including the
    memoryChunk_t header in bytes.

    The "offset" is the shared memory segment offset location where the
    memoryChunk_t header block begins.

    The "type" is the identifier of whether this memory block is located in
    the user shared memory segment or the system shared memory segment.

    The "data" is the fist location that is usable by the user.  This is the
    address that will be returned to the user from his "sm_malloc" function
    call.  The "alignment" attribute is there to ensure that the "data"
    address which will be returned to the user as the result of his
    "sm_malloc" call is aligned on an 8 byte boundary.

------------------------------------------------------------------------*/
typedef struct memoryChunk_t
{
    unsigned long marker;
    unsigned long segmentSize;
    unsigned long offset;
    unsigned int  type;
    char          data[0] __attribute__ ((aligned (8)));

}   memoryChunk_t;


//
//  Define the constants used with the memoryChunk structure.
//
static const unsigned long SM_IN_USE_MARKER  = 0xdeadbeefdeadbeef;
static const unsigned long SM_FREE_MARKER    = 0xfceefceefceefcee;
static const unsigned long CHUNK_HEADER_SIZE = sizeof(memoryChunk_t);
static const unsigned long SPLIT_THRESHOLD   = 16;


/*!-----------------------------------------------------------------------

    s i g n a l L i s t _ t

	@brief Define the structure of each signal list structure.

    This function will define the structure of each signal list in the system.
    A signal list is a list of signals that all have the same domain and key
    values and therefore should be kept together.

    The main data structure of the user shared memory system is a B-tree data
    struture that contains the offsets of these signal list structures.  This
    allows us to quickly store/retrieve the signal list for a given domain and
    key value and allows an unlimited number of unique domain/key combinations
    to be stored in the system.

------------------------------------------------------------------------*/
typedef struct signalList_t
{
    //
    //  Define the signal domain and ID values for all of the signals that
    //  will be stored in this signal list.
    //
	domain_t domain;
	vsiKey_t key;

	//
	//	Define the variables that will be used to keep track of the linked
	//	list data within each signal list.
	//
	//	Note: The "head" is the offset to the first available message in the
	//	current list and the "tail" is the offset to the last available
	//	message in the list (i.e. NOT the next available message slot).  This
	//	is required because we need to be able to update the "next" pointer in
	//	the last message whenever we append a new message to the list and we
	//	can't back up in the list (it's a singly linked list).
	//
	offset_t head;
	offset_t tail;

    //
    //  The following fields are just for informational use.  We probably
    //  don't need them but they could come in handy.  The total size field is
    //  the sum of all of the message fields in this list but this does not
    //  count the signal data header structure sizes in the sum.
    //
	unsigned long currentSignalCount;
	unsigned long totalSignalSize;

	//
    //	Define the semaphore that will be used to manage the processes waiting
    //	for messages on the message queue.  Each message that is received will
    //	increment the semaphore and each "wait" that is executed will
    //	decrement the semaphore.
	//
    semaphore_t semaphore;

}	signalList_t, *signalList_p;

#define SIGNAL_LIST_SIZE   ( sizeof(signalList_t) )
#define END_OF_LIST_MARKER ( 0 )

/*!-----------------------------------------------------------------------

    s i g n a l D a t a _ t
TODO: REDO!
	@brief Define the structure of the header of each signal data item.

    This message header contains all of the information required to manage a
    message in the shared memory structures.  This "shared message" structure
    is inherently variable in size, depending on how much "data" is being
    stored by the calling user.  This message header will be prepended to the
    data that the user asked us to store.

    The "vsiKey_t" field is the primary indexing value for this message.  It
    is defined here as an unsigned 64 bit numberic value.  Keys must be unique
    within a domain for all messages that represent the same type of data.

    The "domain" is a small integer used to identify the domain of a key.  The
    domain is intended as an identifier that will be the same for all messages
    that belong to that domain.  For instance, all messages coming from the
    CAN bus will be in the CAN domain even though there may be 1500 or more
    distinct types of CAN messages.  Each distinct type within the CAN domain
    is identified by a unique "key" value.

    The "messageSize" field is the number of bytes of data that is being
    stored in the "data" field of this structure.  This value does not include
    this message header size, just the amount of data stored in this record.

    The "nextMessageOffset" is the offset within this signal list where the
    "next" message in a message list is stored.  Traversing down the list from
    the signal list "head" field will allow all of the messages in this hash
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
typedef struct signalData_t
{
	offset_t nextMessageOffset;
	offset_t messageSize;
	char     data[0];

}   signalData_t, *signalData_p;

//
//	Define the size of the shared message header structure.  This value needs
//	to be taken into account when the total size of a message is computed.
//	The "messageSize" field in this header is the size of the "data" field at
//	the end of the structure so to get the total size of a structure, we must
//	use:
//
//    totalMessageSize = messagePtr->messageSize + SIGNAL_DATA_HEADER_SIZE;
//
#define SIGNAL_DATA_HEADER_SIZE ( sizeof(signalData_t) )


/*!---------------------------------------------------------------------------

	s h a r e d M e m o r y _ t

	@brief	Define the structure of the shared memory segment.

	This data structure defines the base shared memory segment and is used as
    the shared memory control structure by all of the shared memory functions.

----------------------------------------------------------------------------*/
typedef struct sharedMemory_t
{
    //
    //  Define the base address and size of the shared memory segment.  Also
    //  define the current size to account for memory used to initialize the
    //  system.  Note that we need to keep the original size for use when we
    //  want to unmap or remap the shared memory segment.
    //
    //  The base address may vary from process to process depending on how the
    //  virtual memory of each process that maps this shared memory segment is
    //  set up.  This value is actually the return value from the "mmap"
    //  function call that maps the shared memory segment into the current
    //  process address space.
    //
    unsigned long sharedMemorySegmentSize;
    offset_t      currentOffset;
    unsigned long currentSize;
    unsigned long systemInitialized;

    //
    //  Create the mutex attribute initializer that we can use to initialize
    //  all of the mutexes in the B-trees.
    //
    pthread_mutexattr_t masterMutexAttributes;

    //
    //  Create the condition variable attribute initializer that we can use to
    //  initialize all of the condition variables in the B-trees.
    //
	pthread_condattr_t masterCvAttributes;

    //
    //  Define the mutex that will be used to control access to the shared
    //  memory manager memory management code.  This lock will essentially
    //  lock BOTH of the above B-trees to prevent those B-trees from getting
    //  out of sync with each other.
    //
    pthread_mutex_t smLock;

    //
    //  Define the main data structure for the shared memory segment.  Note
    //  that this data is not treated as part of the basic infrastructure but
    //  rather the first layer of user defined data.  While the 2 B-trees
    //  above are initialized during system initialization, the following data
    //  structures are initialized at a later time after the infrastructure
    //  has completed it's initialization.
    //
    btree_t signals;

#ifdef VSI_DEBUG
    //
    //  The following is for debugging only.  It was used to timestamp events
    //  in a multiple process/thread environment when it was necessary to
    //  determine the absolute timing (i.e. order) in which events occurred.
    //  This variable is the base time that gets set when the process first
    //  starts.  All other times are computed relative to this value.
    //
	unsigned long globalTime;
#endif

}	sharedMemory_t, *sharedMemory_p;


/*!---------------------------------------------------------------------------

    s y s M e m o r y _ t

----------------------------------------------------------------------------*/
typedef struct sysMemory_t
{
    //
    //  Define the base address and size of the shared memory segment.  Also
    //  define the current size to account for memory used to initialize the
    //  system.  Note that we need to keep the original size for use when we
    //  want to unmap or remap the shared memory segment.
    //
    //  The base address may vary from process to process depending on how the
    //  virtual memory of each process that maps this shared memory segment is
    //  set up.  This value is actually the return value from the "mmap"
    //  function call that maps the shared memory segment into the current
    //  process address space.
    //
    unsigned long sharedMemorySegmentSize;
    offset_t      currentOffset;
    unsigned long currentSize;
    offset_t      freeListHead;
    offset_t      freeListTail;
    unsigned long freeListCount;
    unsigned long systemInitialized;

    //
    //  Create the mutex attribute initializer that we can use to initialize
    //  all of the mutexes in the B-trees.
    //
    pthread_mutexattr_t masterMutexAttributes;

    //
    //  Create the condition variable attribute initializer that we can use to
    //  initialize all of the condition variables in the B-trees.
    //
	pthread_condattr_t masterCvAttributes;

    //
    //  Define the mutex that will be used to control access to the shared
    //  memory manager memory management code.  This lock will essentially
    //  lock BOTH of the above B-trees to prevent those B-trees from getting
    //  out of sync with each other.
    //
    pthread_mutex_t sysLock;

    //
    //  Define the B-trees that will be used for the shared memory memory
    //  management.
    //
    btree_t availableMemoryBySize;
    btree_t availableMemoryByOffset;

}   sysMemory_t, *sysMemory_p;


/*!-----------------------------------------------------------------------

	S i g n a l   L i s t   m e m b e r   f u n c t i o n s

------------------------------------------------------------------------*/
//
//	Return the address of a message from the specified signal list.
//
//	This function will return the actual pointer into the shared memory
//	segment that corresponds to the location of the "offset" within the
//	specified signal list.  These pointers are necessary when we want to
//	actually read and write data into the messages being stored.
//
inline static void* toAddress ( offset_t offset )
{
	return (void*)( (void*)smControl + offset );
}


inline static offset_t toOffset ( void* address )
{
    return (offset_t)(address - (void*)smControl);
}


/*!-----------------------------------------------------------------------

    S h a r e d   M e m o r y   M e m b e r   F u n c t i o n s

------------------------------------------------------------------------*/
//
//	Initialize the shared memory segments.
//
//  These functions will take a newly created shared memory segment file
//  descriptor and size, map the file into memory, and initialize all of the
//  fields and variables in the structure.
//
sharedMemory_p sm_initialize ( int fd, size_t sharedMemorySegmentSize );

sysMemory_p sm_initialize_sys ( int fd, size_t sharedMemorySegmentSize );


//
//  Find an available chunk of memory of an appropriate size and return the
//  virtual memory address of it to the caller.  This will mark that section
//  of memory as "allocated".
//
void* sm_malloc     ( size_t size );
void* sm_malloc_sys ( size_t size );


//
//  Give the specified chunk of memory back to the page manager.  This will
//  mark the page as "available" and erase all data in the page (for security
//  reasons).  Once a page has been "freed", it should no longer be referenced
//  by any applications and attempting to dereference it will result in an
//  error (or more likely a SEGV error!).
//
void sm_free     ( void* memoryToFree );
void sm_free_sys ( void* memoryToFree );


//
//  Insert a record into the user data store.
//
//  This function will find the appropriate signal list for the given key and
//  domain and insert the caller's data into the message list of that hash
//  bucket.
//
int sm_insert ( domain_t domain, vsiKey_t key, unsigned long newMessageSize,
                void* body );

//
//	Remove the first message from the given signal list.
//
//  This function will remove the signal at the beginning of the specified
//  signal list.
//
//	WARNING: This function assumes that the signal list mutex has been
//	acquired before this function is called.
//
int sm_removeSignal ( signalList_p signalList );


//
//	Fetch the oldest available record from the user data store.
//
//	This function will find the oldest record in the data store with the
//	specified domain and key and return a copy of it to the caller.
//
//  Note that this function will remove the record that was found from the
//  data store.
//
int sm_fetch ( domain_t domain, vsiKey_t key, unsigned long* messageSize,
               void* body, bool dontWait );

//
//	Fetch the newest available record from the user data store.
//
//	This function will find the newest record in the data store with the
//	specified domain and key and return a copy of it to the caller.
//
//  Note that this function will NOT remove the record that was found from the
//  data store.
//
int sm_fetch_newest ( domain_t domain, vsiKey_t key, unsigned long* messageSize,
                      void* body, bool dontWait );

//
//  Flush all signals in the data store with the given domain and key value.
//
//  This function will find all of the signals with the given domain and key
//  vaue and remove them from the data store.
//
int sm_flush_signal ( domain_t domain, vsiKey_t key );


//
//  Declare the dumping debugging functions.
//
void dumpSM           ( void );
void dumpFreeBySize   ( void );
void dumpFreeByOffset ( void );
void dumpAllSignals   ( int maxSignals );


#endif	// End of #ifndef SHARED_MEMORY_H

/*! @} */

// vim:filetype=h:syntax=c
