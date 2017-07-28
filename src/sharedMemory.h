/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

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
//  Include the local header files we need.
//
#include "vsi.h"
#include "sharedMemoryLocks.h"
#include "btree.h"


//
//  Define the global constants and variables.
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
//  TODO: The resizing of the shared memory segments needs to be written!
//
#define MB ( 1024L * 1024L )
#define SHARED_MEMORY_SEGMENT_NAME "/var/run/shm/vsiUserDataStore"
#define INITIAL_SHARED_MEMORY_SIZE ( 20 * MB )

#define SYS_SHARED_MEMORY_SEGMENT_NAME "/var/run/shm/vsiSysDataStore"
#define SYS_INITIAL_SHARED_MEMORY_SIZE ( 10 * MB )

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
//  Define the type to be used for all of the offset references inside the
//  shared memory segment.
//
typedef unsigned long offset_t;
// typedef unsigned long domain_t;
// typedef unsigned long vsiKey_t;


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
static const unsigned long SM_IN_USE_MARKER  = (unsigned long)0xdeadbeefdeadbeef;
static const unsigned long SM_FREE_MARKER    = (unsigned long)0xfceefceefceefcee;
static const unsigned long CHUNK_HEADER_SIZE = sizeof(memoryChunk_t);
static const unsigned long SPLIT_THRESHOLD   = 16;


/*!---------------------------------------------------------------------------

    s h a r e d M e m o r y _ t

    @brief  Define the structure of the shared memory segment.

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
    //  The following is used to access the VSI specific data structures.
    //
    //  There is an offset variable that will contain the offset into the
    //  shared memory segment where the VSI context structure is located
    //  persistently. There is a global pointer variable that is set to the
    //  actual memory address of the VSI context structure in the current
    //  process.  This second variable is set each time the system is
    //  initialzed or just opened and is provided as a convenience for users
    //  so the offset value doesn't need to be converted to an address
    //  continually.
    //
    unsigned long vsiContextOffset;

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

#ifdef LOG
    //
    //  The following is for debugging only.  It was used to timestamp events
    //  in a multiple process/thread environment when it was necessary to
    //  determine the absolute timing (i.e. order) in which events occurred.
    //  This variable is the base time that gets set when the process first
    //  starts.  All other times are computed relative to this value.
    //
    unsigned long globalTime;
#endif

}   sharedMemory_t, *sharedMemory_p;


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

    //
    //  Define the B-tree key field definition structures.
    //
    //  Note that these can't be defined easily because the btree_key_def
    //  structure contains a variable sized array of btree_field_def
    //  structures.  To get around that, we'll define a structure here that
    //  mimics what the compiler would do to build these nested structures and
    //  then we'll initialize that structure during the runtime initialization
    //  phase.
    //
    //  So what we want to wind up with after initialization is something that
    //  looks like the following:
    //
    //    btree_key_def sizeKeyDef =
    //    {
    //        2,
    //        btree_field_def field1Def1 =
    //        {
    //            ft_ulong,
    //            offsetof ( memoryChunk_t, segmentSize ),
    //            1
    //        };
    //        btree_field_def field1Def2 =
    //        {
    //            ft_ulong,
    //            offsetof ( memoryChunk_t, offset ),
    //            1
    //        };
    //    };
    //
    //    btree_key_def offsetKeyDef =
    //    {
    //        1,
    //        btree_field_def field1Def =
    //        {
    //            ft_ulong,
    //            offsetof ( memoryChunk_t, offset ),
    //            1
    //        };
    //    };
    //
    //  These data structures will be needed to initialize the "by size" and
    //  "by offset" b-tree structures that are used by the shared memory
    //  memory allocators.
    //
    btree_key_def sizeKeyDef;
    btree_field_def sizeFieldDef1;
    btree_field_def sizeFieldDef2;

    btree_key_def offsetKeyDef;
    btree_field_def offsetFieldDef;

}   sysMemory_t, *sysMemory_p;


/*!-----------------------------------------------------------------------

    S h a r e d   M e m o r y   M e m b e r   F u n c t i o n s

------------------------------------------------------------------------*/
//
//  Return the address of a message from the specified signal list.
//
//  This function will return the actual pointer into the shared memory
//  segment that corresponds to the location of the "offset" within the
//  specified signal list.  These pointers are necessary when we want to
//  actually read and write data into the messages being stored.
//
inline static void* toAddress ( offset_t offset )
{
    return (void*)( (void*)smControl + offset );
}


inline static offset_t toOffset ( void* address )
{
    return (offset_t)(address - (void*)smControl);
}


//
//  Initialize the shared memory segments.
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


#if 0
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
//  Remove the first message from the given signal list.
//
//  This function will remove the signal at the beginning of the specified
//  signal list.
//
//  WARNING: This function assumes that the signal list mutex has been
//  acquired before this function is called.
//
int sm_removeSignal ( signalList_p signalList );


//
//  Fetch the oldest available record from the user data store.
//
//  This function will find the oldest record in the data store with the
//  specified domain and key and return a copy of it to the caller.
//
//  Note that this function will remove the record that was found from the
//  data store.
//
int sm_fetch ( domain_t domain, vsiKey_t key, unsigned long* messageSize,
               void* body, bool dontWait );

//
//  Fetch the newest available record from the user data store.
//
//  This function will find the newest record in the data store with the
//  specified domain and key and return a copy of it to the caller.
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
#endif


//
//  Declare the dumping debugging functions.
//
void dumpSM           ( void );
void dumpFreeBySize   ( void );
void dumpFreeByOffset ( void );


#endif  // End of #ifndef SHARED_MEMORY_H

/*! @} */

// vim:filetype=h:syntax=c
