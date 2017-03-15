/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

	@file sharedMemory.c

	This file contains the functions that manipulate the shared memory segment
	data structures.  Most of the major functions defined in the VSI are
	defined here.

-----------------------------------------------------------------------------*/

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "vsi_core_api.h"
#include "sharedMemory.h"


/*! @{ */


//
//  Declare the addresses of the global shared memory control structures.
//
struct sharedMemory_t* smControl  = 0;
struct sysMemory_t*    sysControl = 0;


//
//  Define the size of the shared memory control block, making sure they are
//  multiples of 8 (for memory alignment).
//
static const unsigned int smSize  = ( sizeof(sharedMemory_t) + 7 ) & 0xfffffff8;
static const unsigned int sysSize = ( sizeof(sysMemory_t)    + 7 ) & 0xfffffff8;

//
//  Declare the local functions.
//
static void memoryChunkPrint ( char* leader, void* recordPtr );
// static void signalListPrint  ( char* leader, void* recordPtr );


//
//  Define the cleanup handler for the semaphore wait below.  This handler
//  will be executed if this thread is cancelled while it is waiting.  What
//  this function needs to do is release the mutex that this thread is holding
//  when it enters the wait state.  Without this cleanup handler, the mutex
//  will remain locked when this thread is cancelled and almost certainly
//  cause a hang condition in the code the next time someone tries to lock
//  this same mutex.
//
static inline void semaphoreCleanupHandler ( void* arg )
{
    pthread_mutex_unlock ( arg );
}


/*!-----------------------------------------------------------------------

	S M _ L O C K

    @brief Acquire the shared memory control structure lock.

    This function will hang if the lock is currently not available and return
    when the lock has been successfully acquired.

    NOTE: It is REQUIRED that the "push" and "pop" cleanup functions be
    executed in the same lexical nesting level in the same function so the
    "lock" and "unlock" macros must be used at the same lexical nesting level
    and in the same function as well.  These functions also had to be
    implemented as macros instead of functions because of the lexical level
    limitation.

------------------------------------------------------------------------*/
#define SM_LOCK                                                        \
    if ( ( status = pthread_mutex_lock ( &smControl->smLock ) ) != 0 ) \
    {                                                                  \
        printf ( "Error: Unable to acquire the shared memory manager " \
                 "lock: %d[%s]\n", status, strerror(status) );         \
    }                                                                  \
    pthread_cleanup_push ( semaphoreCleanupHandler, &smControl->smLock );


/*!-----------------------------------------------------------------------

	S M _ U N L O C K

    @brief Release the shared memory control structure lock.

    This function will unlock the mutex on the signal list.  If another
    process or thread is waiting on this lock, the scheduler will decide which
    will run so the order of processes/threads may not be the same as the
    order in which they called the lock function.

------------------------------------------------------------------------*/
#define SM_UNLOCK                             \
    pthread_mutex_unlock ( &smControl->smLock ); \
    pthread_cleanup_pop ( 0 );


/*!-----------------------------------------------------------------------

    d e l e t e M e m o r y C h u n k

	@brief Delete a chunk of memory from both indices.

	This function will delete a chunk of memory from both indices.

	@param[in] - The header of the memory chunk to delete.

	@return  0 = Success
            ~0 = Failure (errno value)

------------------------------------------------------------------------*/
static int deleteMemoryChunk ( memoryChunk_t* chunk )
{
    int status = 0;

    //
    //  Go remove the given memory chunk from the bySize index.
    //
    status = btree_delete ( &sysControl->availableMemoryBySize, chunk );
    if ( status != 0 )
    {
        printf ( "Error: Could not delete memory chunk by size at offset %lx "
                 "- Aborting\n", chunk->offset );
        return status;
    }
    //
    //  Go remove the given memory chunk from the byOffset index.
    //
    status = btree_delete ( &sysControl->availableMemoryByOffset, chunk );
    if ( status != 0 )
    {
        printf ( "Error: Could not delete memory chunk by offset at %lx - "
                 "Aborting\n", chunk->offset );
    }
    return status;
}


/*!-----------------------------------------------------------------------

    i n s e r t M e m o r y C h u n k

	@brief Insert a chunk of memory from both indices.

	This function will insert a chunk of memory from both indices.

	@param[in] - The header of the memory chunk to insert

	@return  0 = Success
            ~0 = Failure (errno value)

------------------------------------------------------------------------*/
static int insertMemoryChunk ( memoryChunk_t* chunk )
{
    int status = 0;

    //
    //  Go insert the given memory chunk into the bySize index.
    //
    status = btree_insert ( &sysControl->availableMemoryBySize, chunk );
    if ( status != 0 )
    {
        printf ( "Error: Could not insert memory chunk by size at offset %lx "
                 "- Aborting\n", chunk->offset );
        return status;
    }
    //
    //  Go insert the given memory chunk into the byOffset index.
    //
    status = btree_insert ( &sysControl->availableMemoryByOffset, chunk );
    if ( status != 0 )
    {
        printf ( "Error: Could not insert memory chunk by offset at %lx - "
                 "Aborting\n", chunk->offset );
    }
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ i n i t i a l i z e

	@brief Initialize the data structures in a new shared memory segment.

	The specified file (referenced by the supplied file descriptor) will be
	mapped into virtual memory and the size of the file adjusted to match the
	desired size of the shared memory segment.

	This function will set up all of the data structures in the shared memory
	segment for use.  This function should be called after a brand new shared
	memory segment has been created and opened on disk.  This function should
	not be called on an existing shared memory segment that contains data as
	all of that data will be deleted by this function.

	@param[in] fd - The file descriptor of the file associated with this
			   shared memory segment.

	@param[in] sharedMemorySegmentSize - The size of the shared memory
			   segment in bytes.

	@return The new memory address of where the shared memory segment begins.
	        On error, a null pointer is returned and errno will be set to
			indicate the error that was encountered.

------------------------------------------------------------------------*/
sharedMemory_p sm_initialize ( int fd, size_t sharedMemorySegmentSize )
{
	int            status;
	sharedMemory_p sharedMemory;

    LOG ( "Initializing user shared memory - fd[%d], Size[%'lu]\n", fd,
          sharedMemorySegmentSize );
	//
	//	Map the shared memory file into virtual memory.
	//
	sharedMemory = mmap ( NULL, INITIAL_SHARED_MEMORY_SIZE,
                          PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0 );
	if ( sharedMemory == MAP_FAILED )
	{
		printf ( "Unable to map the user shared memory segment. errno: %u[%m].\n",
				 errno );
		return 0;
	}
    //
	//	Make the pseudo-file for the shared memory segment the size of the
	//	shared memory segment.  Note that this will destroy any existing data
	//	if the segment already exists.
    //
    status = ftruncate ( fd, INITIAL_SHARED_MEMORY_SIZE );
    if (status != 0)
    {
        printf ( "Unable to resize the user shared memory segment to [%lu] bytes - "
                 "errno: %u[%m].\n", INITIAL_SHARED_MEMORY_SIZE, errno );
        return 0;
    }
	//
	//	Set all of the data in this shared memory segment to zero.
	//
	// TODO: (void)memset ( sharedMemory, 0, sharedMemorySegmentSize );
	(void)memset ( sharedMemory, 0xaa, sharedMemorySegmentSize );

    //
    //  Initialize our global pointer to the shared memory segment.
    //
    smControl = sharedMemory;

    //
    //  Initialize the fields in the shared memory control structure.  This
    //  structure is located at the beginning of the shared memory region that
    //  we just created and the fields here are adjusted to take into account
    //  the size of the user shared memory structure.
    //
    sharedMemory->sharedMemorySegmentSize = sharedMemorySegmentSize;
    sharedMemory->currentOffset           = smSize;
    sharedMemory->currentSize             = sharedMemorySegmentSize - smSize;
    sharedMemory->systemInitialized       = 0;

	//
	//	Create the mutex attribute initializer that we can use to initialize
	//	all of the mutexes in the B-trees.
    //
	pthread_mutexattr_t* mutexAttributes = &sharedMemory->masterMutexAttributes;

    status =  pthread_mutexattr_init ( mutexAttributes );
    if ( status != 0 )
    {
        printf ( "Unable to initialize mutex attributes - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    // Set this mutex to be a process shared mutex.
    //
    status = pthread_mutexattr_setpshared ( mutexAttributes,
											PTHREAD_PROCESS_SHARED );
    if ( status != 0 )
    {
        printf ( "Unable to set shared mutex attribute - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    // Set this mutex to be a recursive mutex.
    //
    status = pthread_mutexattr_settype ( mutexAttributes,
										 PTHREAD_MUTEX_RECURSIVE );
    if ( status != 0 )
    {
        printf ( "Unable to set recursive mutex attribute - errno: %u[%m].\n",
                 status );
        return 0;
    }
	//
	//	Create the condition variable initializer that we can use to
	//	initialize all of the condition variables in the B-trees.
	//
	pthread_condattr_t* conditionVariableAttributes =
        &sharedMemory->masterCvAttributes;

	status = pthread_condattr_init ( conditionVariableAttributes );

    if ( status != 0 )
    {
        printf ( "Unable to initialize condition variable attributes - "
			     "errno: %u[%m].\n", status );
        return 0;
    }
	//
	//	Set this condition variable to be shared between processes.
	//
	status = pthread_condattr_setpshared ( conditionVariableAttributes,
										   PTHREAD_PROCESS_SHARED );
    if ( status != 0 )
    {
        printf ( "Unable to set shared condition variable attributes - "
				 "errno: %u[%m].\n", status );
        return 0;
    }
    //
    //  Initialize the shared memory mutex.
    //
    status = pthread_mutex_init ( &sharedMemory->smLock, mutexAttributes );
    if ( status != 0 )
    {
        printf ( "Unable to initialize shared memory manager mutex - errno: "
                 "%u[%m].\n", status );
        return 0;
    }
    //
    //  Now put the rest of the preallocated memory into the free memory pool.
    //
    //  Define the memory chunk data structure at the beginning of the free
    //  section of memory and initialize it.
    //
    memoryChunk_t* availableMemory = toAddress ( smControl->currentOffset );

    availableMemory->marker      = SM_FREE_MARKER;
    availableMemory->segmentSize = smControl->currentSize;
    availableMemory->offset      = smControl->currentOffset;
    availableMemory->type        = TYPE_USER;

    //
    //  Go insert this memory into the shared memory allocation B-trees.
    //
    (void)insertMemoryChunk ( availableMemory );

    //
    //  Set the "initialized" flag to indicate that the shared memory
    //  allocation system is in a usable state.  Note that this indicator is
    //  primarily used by the sm_malloc function to determine how to allocate
    //  memory for the shared memory memory management function B-trees that
    //  we just defined above.
    //
    sharedMemory->systemInitialized = 1;

    //
    //  Go initialize the B-tree that will manage the signal data.
    //
    //  TODO: Move this to the vsi module code.
    btree_create_in_place ( &smControl->signals,
                            SM_SIGNALS_RECORD_COUNT,
                            SM_SIGNALS_KEY_COUNT,
                            SM_SIGNALS_KEY1,
                            SM_SIGNALS_KEY2,
                            0, 0 );
    //
    //  Let the user know that the system initialization has been completed.
    //
    LOG ( "\n===== User shared memory is Initialized at %p =====\n",
          sharedMemory );

#ifdef VSI_DEBUG
    dumpSM();
#endif

	//
	//	Return the address of the shared memory segment to the caller.
	//
	return sharedMemory;
}


/*!-----------------------------------------------------------------------

TODO: FIX!
    s m _ i n i t i a l i z e _ s y s

	@brief Initialize the data structures in a new shared memory segment.

	The specified file (referenced by the supplied file descriptor) will be
	mapped into virtual memory and the size of the file adjusted to match the
	desired size of the shared memory segment.

	This function will set up all of the data structures in the shared memory
	segment for use.  This function should be called after a brand new shared
	memory segment has been created and opened on disk.  This function should
	not be called on an existing shared memory segment that contains data as
	all of that data will be deleted by this function.

	@param[in] fd - The file descriptor of the file associated with this
			   shared memory segment.

	@param[in] sharedMemorySegmentSize - The size of the shared memory
			   segment in bytes.

	@return The new memory address of where the shared memory segment begins.
	        On error, a null pointer is returned and errno will be set to
			indicate the error that was encountered.

------------------------------------------------------------------------*/
sysMemory_p sm_initialize_sys ( int fd, size_t sharedMemorySegmentSize )
{
	int         status;
	sysMemory_p sharedMemory;

    LOG ( "Initializing system shared memory - fd[%d], Size[%'lu]\n", fd,
          sharedMemorySegmentSize );
	//
	//	Map the shared memory file into virtual memory.
	//
	sharedMemory = mmap ( NULL, SYS_INITIAL_SHARED_MEMORY_SIZE,
                          PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0 );
	if ( sharedMemory == MAP_FAILED )
	{
		printf ( "Unable to map the system shared memory segment. errno: %u[%m].\n",
				 errno );
		return 0;
	}
    //
	//	Make the pseudo-file for the shared memory segment the size of the
	//	shared memory segment.  Note that this will destroy any existing data
	//	if the segment already exists.
    //
    status = ftruncate ( fd, SYS_INITIAL_SHARED_MEMORY_SIZE );
    if (status != 0)
    {
        printf ( "Unable to resize the system shared memory segment to [%lu] "
                 "bytes - errno: %u[%m].\n", SYS_INITIAL_SHARED_MEMORY_SIZE,
                  errno );
        return 0;
    }
	//
	//	Set all of the data in this shared memory segment to zero.
	//
	// TODO: (void)memset ( sharedMemory, 0, sharedMemorySegmentSize );
	(void)memset ( sharedMemory, 0xbb, sharedMemorySegmentSize );

    //
    //  Initialize our global pointer to the shared memory segment.
    //
    sysControl = sharedMemory;

    //
    //  Initialize the fields in the shared memory control structure.  This
    //  structure is located at the beginning of the shared memory region that
    //  we just created and the fields here are adjusted to take into account
    //  the size of the system shared memory structure.
    //
    sharedMemory->sharedMemorySegmentSize = sharedMemorySegmentSize;
    sharedMemory->currentOffset           = sysSize;
    sharedMemory->currentSize             = sharedMemorySegmentSize - sysSize;
    sharedMemory->systemInitialized       = 0;

	//
	//	Create the mutex attribute initializer that we can use to initialize
	//	all of the mutexes in the B-trees.
    //
	pthread_mutexattr_t* mutexAttributes = &sharedMemory->masterMutexAttributes;

    status =  pthread_mutexattr_init ( mutexAttributes );
    if ( status != 0 )
    {
        printf ( "Unable to initialize system mutex attributes - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    // Set this mutex to be a process shared mutex.
    //
    status = pthread_mutexattr_setpshared ( mutexAttributes,
											PTHREAD_PROCESS_SHARED );
    if ( status != 0 )
    {
        printf ( "Unable to set system shared mutex attribute - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    // Set this mutex to be a recursive mutex.
    //
    status = pthread_mutexattr_settype ( mutexAttributes,
										 PTHREAD_MUTEX_RECURSIVE );
    if ( status != 0 )
    {
        printf ( "Unable to set system recursive mutex attribute - errno: %u[%m].\n",
                 status );
        return 0;
    }
    //
    //  Initialize the shared memory mutex.
    //
    status = pthread_mutex_init ( &sharedMemory->sysLock, mutexAttributes );
    if ( status != 0 )
    {
        printf ( "Unable to initialize system shared memory manager mutex - errno: "
                 "%u[%m].\n", status );
        return 0;
    }
    //
    //  Now we can divide up the rest of the system shared memory segment into
    //  the free B-tree blocks list.
    //
    //  We do this by creating a singly linked list of blocks using the blocks
    //  themselves containing the offset to the next block.
    //
	//  Now compute the size of each node in the system B-trees...
    //
    //  If the user specified an even number of records, increment it to be an
    //  odd number.  The btree algorithm here only operates correctly if the
    //  record count is odd.
	//
    unsigned int maxRecordsPerNode = SYS_RECORD_COUNT;

	if ( ( maxRecordsPerNode & 1 ) == 0 )
	{
		maxRecordsPerNode++;
	}
	//
	//  Compute the node size as the btree node header size plus the size of
	//  the record offset area plus the link offfset area.
	//
    unsigned int nodeSize = sN + ( maxRecordsPerNode * sO ) +
					        ( ( maxRecordsPerNode + 1 ) * sO );
    //
    //  Round up the node size to the next multiple of 8 bytes to maintain
    //  long int alignment in the structures.
    //
    nodeSize = ( nodeSize + 7 ) & 0xfffffff8;

    //
    //  Round up the current free memory offset to the next multiple of 8
    //  bytes.
    //
    offset_t currentOffset = ( sysControl->currentOffset + 7 ) & 0xfffffff8;

    //
    //  Adjust the number of bytes of free memory available after the rounding
    //  up of the offset above.
    //
    unsigned int currentSize =
        sysControl->currentSize - ( currentOffset - sysControl->currentOffset );

    //
    //  Initialize the free list head offset to point to the first block in
    //  our free list.
    //
    sysControl->freeListHead = currentOffset;

    //
    //  Initialize the free list block counter.
    //
    sysControl->freeListCount = 0;

    //
    //  Get the pointer to the first free memory block.
    //
    void* currentPtr = (void*)sysControl + sysSize;

    //
    //  while we have not exhausted the amount of memory in this shared memory
    //  segment...
    //
    offset_t workingOffset = currentOffset;
    while ( currentSize >= nodeSize )
    {
        //
        //  Store the offset to the next block in the current block.
        //
        workingOffset += nodeSize;
        *(offset_t*)currentPtr = workingOffset;

        //
        //  Increment the current memory pointer and decrement the amount of
        //  space we still have left.
        //
        currentPtr  += nodeSize;
        currentSize -= nodeSize;

        //
        //  Increment the number of blocks in the free list.
        //
        ++sysControl->freeListCount;
    }
    //
    //  We have finished setting up the list of free blocks so set the tail
    //  pointer to point to the last block and mark the last block with the
    //  NULL offset value.
    //
    sysControl->freeListTail = workingOffset;
    *(offset_t*)currentPtr = 0;

    //
    //  Set the "initialized" flag to indicate that the shared memory
    //  allocation system is in a usable state.  Note that this indicator is
    //  primarily used by the sm_malloc function to determine how to allocate
    //  memory for the shared memory memory management function B-trees that
    //  we just defined above.
    //
    sharedMemory->systemInitialized = 1;

    //
    //  Now go initialize the 2 B-trees that will be used to perform the
    //  memory management in the user shared memory segment.
    //
    sysControl->availableMemoryBySize.type = TYPE_SYSTEM;
    btree_create_sys ( &sysControl->availableMemoryBySize,
                       SYS_RECORD_COUNT,
                       SYS_SIZE_KEY_COUNT,
                       SYS_SIZE_KEY1,
                       SYS_SIZE_KEY2,
                       0, 0 );

    sysControl->availableMemoryByOffset.type = TYPE_SYSTEM;
    btree_create_sys ( &sysControl->availableMemoryByOffset,
                       SYS_RECORD_COUNT,
                       SYS_OFFSET_KEY_COUNT,
                       SYS_OFFSET_KEY1,
                       SYS_OFFSET_KEY2,
                       0, 0 );
    //
    //  Let the user know that the system initialization has been completed.
    //
    LOG ( "\n===== System shared memory is Initialized at %p =====\n\n",
          sharedMemory );
	//
	//	Return the address of the shared memory segment to the caller.
	//
	return sharedMemory;
}


/*!----------------------------------------------------------------------------

    s m _ m a l l o c

	@brief Allocate a chunk of shared memory for a user.

    This function will allocate a chunk of shared memory of the specified size
    (in bytes).  If no memory is available of the specified size, a NULL
    pointer will be returned, otherwise, the address (not the offset) of the
    allocated chunk will be returned to the caller.

    This function operates identically to the system "malloc" function and can
    be used the same way.

	@param[in] size - The size in bytes of the memory desired.

	@return The address of the allocated memory chunk.
            NULL if the request could not be honored.

-----------------------------------------------------------------------------*/
void* sm_malloc ( size_t size )
{
    LOG ( "In SM sm_malloc[%lu]\n", size );

#ifdef VSI_DEBUG
    //dumpSM();
#endif

    memoryChunk_t  needed = { 0 };
    memoryChunk_t* found;
    memoryChunk_t* availableChunk;
    unsigned long  neededSize;
    int            status;

    //
    //  Initialize the size of the memory we need.  Note that we need enough
    //  space to insert the chunkHeader_t at the beginning of the block the
    //  user wants.
    //
    neededSize = size + CHUNK_HEADER_SIZE;

    //
    //  Make sure the memory we are going to allocate is on an 8 byte boundary
    //  by rounding it up to the next multiple of 8.
    //
    //  TODO: Do we need to generalize this?  (like using "sizeof(long)", etc.
    //
    neededSize = ( neededSize + 7 ) & 0xfffffffffffffff8;

    //
    //  If the shared memory memory management system has not yet been
    //  initialized, just grab what we need from the beginning of the shared
    //  memory segment.
    //
    if ( ! sysControl->systemInitialized )
    {
        printf ( "Error: sm_malloc called before init finished for size: [%zu]\n",
                 size );
        exit ( 255 );
    }
    //
    //  If we got here we are operating in "normal" mode rather than
    //  "infrastructure initialization" mode.
    //
    //  Lock the mutex that controls the shared memory manager.
    //
    SM_LOCK;

    //
    //  Set up the data structure that we need to search for the appropriately
    //  sized chunk of memory.
    //
    needed.segmentSize = neededSize;
    needed.offset      = 0;

    //
    //  Go find the first block in the available B-tree that is equal to or
    //  greater than the size we need.
    //
    btree_iter iter = btree_find ( &sysControl->availableMemoryBySize, &needed );

    //
    //  If the search didn't find anything then we don't have any memory
    //  chunks large enough to fulfill the request so there is something
    //  seriously wrong and we should just abort for the time being.
    //
    //  TODO: In the future we should be able to extend the shared memory
    //  segment with little overhead.
    //
    if ( btree_iter_at_end ( iter ) )
    {
        printf ( "Error: No memory block of size %zu is available! Aborting\n",
                 size );
        goto mallocEnd;
    }
    //
    //  Get the address of the chunk of memory that we found.
    //
    found = availableChunk = btree_iter_data ( iter );

    //
    //  Go remove the chunk of memory we found from the indices.
    //
    (void)deleteMemoryChunk ( found );

    //
    //  If the memory chunk we found is much larger than what we asked for
    //  then we will need to split the chunk into 2 pieces.  The split
    //  threashold is here to keep us from splitting off a piece of memory
    //  that is so small that it's not useful.  Anything less than the split
    //  threshold will just be left as a single block with some unused space
    //  at the end to cut down on memory fragmentation.
    //
    if ( found->segmentSize - neededSize > SPLIT_THRESHOLD )
    {
        //
        //  Remove the size of the memory chunk we need from the beginning of
        //  what was found and put the remainder back into the "available"
        //  B-tree.
        //
        //  Increment our pointer to the beginning of the leftover memoryChunk
        //  header structure location.
        //
        found = (void*)found + neededSize;

        //
        //  Populate the new header with all the new information for the
        //  leftover piece of memory.
        //
        found->marker      = SM_FREE_MARKER;
        found->offset      = availableChunk->offset + neededSize;
        found->segmentSize = availableChunk->segmentSize - neededSize;
        found->type        = TYPE_SYSTEM;

        //
        //  Go insert the leftover piece of memory back into the indices.
        //
        (void)insertMemoryChunk ( found );

        //
        //  Fix the fields of the initial piece that we split off to be correct
        //  now.
        //
        availableChunk->marker      = SM_IN_USE_MARKER;
        availableChunk->segmentSize = neededSize;
    }
    //
    //  If there is no need to split the piece that we found, just mark it as
    //  "in use".
    //
    else
    {
        availableChunk->marker = SM_IN_USE_MARKER;
    }

mallocEnd:

    //
    //  Go unlock our shared memory mutex.
    //
    SM_UNLOCK;

    //
    //  Return the address of the user data part of the available memory chunk
    //  to the caller.
    //
    printf ( "  sm_malloc returning[%p]\n", &availableChunk->data );

#ifdef VSI_DEBUG
    //
    //  Perform a sanity check on this chunk of memory by checking to make
    //  sure it is not past the end of the shared memory segment.
    //
    if ( (void*)&availableChunk->data >=
         (void*)sysControl + sysControl->sharedMemorySegmentSize )
    {
        printf ( "\n\nERROR!!! sm_malloc retuning a bad address!!\n\n" );
        printf ( "    %lu past end of shared memory segment!\n",
                 (void*)&availableChunk->data -
                 (void*)sysControl->sharedMemorySegmentSize );
    }
#endif
    //
    //  Return the address of the shared memory chunk just allocated to the
    //  caller.
    //
    return &availableChunk->data;
}


/*!----------------------------------------------------------------------------

    s m _ m a l l o c _ s y s

	@brief

	This function will

	@param[in]
	@param[out]
	@param[in,out]

	@return None

-----------------------------------------------------------------------------*/
void* sm_malloc_sys ( size_t size )
{
    LOG ( "In SM sm_malloc_sys[%lu]\n", size );

    void* nodePtr;

    if ( sysControl->freeListHead == 0 )
    {
        printf ( "Error: No B-tree node blocks available\n" );
        return 0;
    }

    nodePtr = sysControl->freeListHead + (void*)sysControl;

    sysControl->freeListHead = *(offset_t*)nodePtr;

#ifdef VSI_DEBUG
    //dumpSM();
#endif

    return nodePtr;
}


/*!----------------------------------------------------------------------------

    s m _ f r e e

	@brief Deallocate a chunk of shared memory.

    This function will deallocate the specified chunk of shared memory
    supplied by the caller.  If the supplied chunk of memory is not a valid
    shared memory block or the block is not currently allocated (i.e. already
    freed), the request will be ignored and an error message generated.

    This function will check each piece of memory that is being free to see if
    it is contiguous to another piece of memory in the free list.  If we find
    a contiguous chunk, the two will be merged into a single larger chunk of
    memory.  This operation will cut down on the memory fragmentation by
    combining smaller pieces of memory into larger ones.

    This function operates identically to the system "free" function and can
    be used the same way.

	@param[in] allocatedMemory - The address of the chunk of shared memory to free.

	@return None

-----------------------------------------------------------------------------*/
void sm_free ( void* userMemory )
{
    LOG ( "In SM sm_free[%p]\n", userMemory );

#ifdef VSI_DEBUG
    //dumpSM();
#endif

    int            status;
    memoryChunk_t* nextChunk;
    memoryChunk_t* prevChunk;

#ifdef VSI_DEBUG
    //
    //  Validate that the memory the user is trying to free is within the
    //  bounds of the shared memory region.
    //
    void* smStart = smControl;
    void* smEnd   = smStart + smControl->sharedMemorySegmentSize;
    if ( userMemory <= smStart || userMemory >= smEnd )
    {
        printf ( "Error: Attempt to sm_free memory[%p] not located in the \n"
                 "       shared memory segment[%p->%p]\n", userMemory, smStart,
                  smEnd );
        exit ( 255 );
        return;
    }
#endif
    //
    //  Get the pointer to the memoryChunk header from the user supplied
    //  memory pointer by backing up the pointer by the size of the
    //  memoryChunk data structure.
    //
    memoryChunk_t* memoryChunk = userMemory - CHUNK_HEADER_SIZE;

    //
    //  If there is no valid SM marker in the header of this piece of memory
    //  then there is a serious error so complain and quit.
    //
    if ( memoryChunk->marker != SM_IN_USE_MARKER )
    {
        if ( memoryChunk->marker != SM_FREE_MARKER )
        {
            printf ( "Error: sm_free was called with [%p] which is NOT an\n"
                     "allocated piece of shared memory (it does not have a "
                     "valid chunk marker).\n",
                     userMemory );
        }
        else
        {
            printf ( "Error: sm_free was called with [%p] which has already "
                     "been freed.\n", userMemory );
        }
        // exit ( 255 );
        return;
    }
    //
    //  Lock the shared memory data structures while we play with the B-trees.
    //
    SM_LOCK;

    //
    //  Check to see if the chunk of memory immediately following this one is
    //  already in the available pool.  If it is then we can coalesce these
    //  two blocks into one larger block.
    //
    nextChunk = (void*)memoryChunk + memoryChunk->segmentSize;
    if ( nextChunk->marker == SM_FREE_MARKER )
    {
        printf ( "  Merging memory with next block\n" );

        //
        //  The next chunk of memory is contiguous to the current one so it is
        //  going to disappear.  We need to first remove it from both indices.
        //
        status = deleteMemoryChunk ( nextChunk );

        //
        //  Add the space that used to be occupied by the following chunk to
        //  our current chunk.
        //
        memoryChunk->segmentSize += nextChunk->segmentSize;
    }
    //
    //  Now check to see if the previous chunk of memory is contiguous with
    //  the current one.
    //
    //  Note that we need to decrement the current chunk offset before the
    //  "rfind" call because that call will do a "<=" comparison and we just
    //  want the "<" operation, otherwise, we will just find the current block
    //  again.  The offset gets incremented again after the "rfind" operation
    //  to restore it's proper value.
    //
    --memoryChunk->offset;

    btree_iter iter = btree_rfind ( &sysControl->availableMemoryByOffset,
                                    memoryChunk );
    ++memoryChunk->offset;

    //
    //  Get the address of the previous memory chunk found in the iterator.
    //
    prevChunk = btree_iter_data ( iter );

    //
    //  If we found a chunk of free memory and it is contiguous with the
    //  beginning of our current chunk then combine those 2 chunks into a
    //  single one.
    //
    if ( prevChunk &&
         ( ( prevChunk->offset + prevChunk->segmentSize ) == memoryChunk->offset ) )
    {
        printf ( "  Merging memory with previous block\n" );

        //
        //  Now since we need to change the size of the previous chunk, delete
        //  the old record, change the size, and add the new record back in.
        //
        (void)deleteMemoryChunk ( prevChunk );

        //
        //  Add the space that used to be occupied by the current chunk to the
        //  previous chunk to coalesce the two pieces into one.
        //
        prevChunk->segmentSize += memoryChunk->segmentSize;

        //
        //  Add the new previous chunk data record to the btree.
        //
        (void)insertMemoryChunk ( prevChunk );
    }
    //
    //  If the previous chunk of memory is not contiguous with the one we want
    //  to free, just add the new chunk to the free list.
    //
    else
    {
        printf ( "  Adding this memory block to free list B-Trees.\n" );

        //
        //  Go put the current chunk of memory into the "free" pool.
        //
        memoryChunk->marker = SM_FREE_MARKER;
        (void)insertMemoryChunk ( memoryChunk );
    }
    //
    //  Unlock the shared memory B-trees.
    //
    SM_UNLOCK;

    //
    //  Return to the caller.
    //
    return;
}


/*!-----------------------------------------------------------------------

    s m _ f r e e _ s y s

	@brief

	This function will

	@param[in]
	@param[out]
	@param[in,out]

	@return None

------------------------------------------------------------------------*/
void sm_free_sys ( void* memoryBlock )
{
    LOG ( "In SM sm_free_sys[%p]\n", memoryBlock );

#ifdef VSI_DEBUG
    //dumpSM();
#endif

    // ###############
}


/*!----------------------------------------------------------------------------

    M e m o r y   A l l o c a t i o n   D e b u g   F u n c t i o n s

	The following functions are designed for debugging purposes and will
    normally not be included in production builds.

-----------------------------------------------------------------------------*/
//
//  Print out a nicely formatted message giving the address, size, and offset
//  of the supplied memory chunk.
//
void memoryChunkPrint ( char* leader, void* recordPtr )
{
    memoryChunk_t* memory = recordPtr;

    if ( memory->marker != SM_FREE_MARKER && memory->marker != SM_IN_USE_MARKER )
    {
        printf ( "Error: Invalid memory chunk at %p\n", recordPtr );
        return;
    }
    printf ( "%sChunk: %p[%lx] - Size[%4lu], Offset[0x%lx]\n", leader, memory,
             toOffset ( recordPtr ), memory->segmentSize, memory->offset );
    return;
}


//
//  Define a default traverse callback function that just prints out the data
//  being pointed to.
//
void memoryChunkTraverse ( char* leader, void* data )
{
    memoryChunkPrint ( "  ", data );
}


//
//  Dump the entire "memory chunk by size" B-tree.
//
void dumpFreeBySize ( void )
{
    //
    //  If the system has not finished initializing yet, just ignore this
    //  call.
    //
    if ( sysControl == NULL )
    {
        return;
    }
    btree_t* btree = &sysControl->availableMemoryBySize;

    printf ( "\nSIZE B-tree: %p, count: %u\n",
             btree, btree->count );

    btree_traverse ( btree, memoryChunkTraverse );
}


//
//  Dump the entire "memory chunk by offset" B-tree.
//
void dumpFreeByOffset ( void )
{
    //
    //  If the system has not finished initializing yet, just ignore this
    //  call.
    //
    if ( sysControl == NULL )
    {
        return;
    }
    btree_t* btree = &sysControl->availableMemoryByOffset;

    printf ( "\nOFFSET B-tree: %p, count: %u\n",
             btree, btree->count );

    btree_traverse ( btree, memoryChunkTraverse );
}


void dumpAllSignals ( int maxSignals )
{
    //
    //  If the system has not finished initializing yet, just ignore this
    //  call.
    //
    if ( sysControl == NULL )
    {
        return;
    }
    btree_t* btree = &smControl->signals;

    printf ( "\nSignls B-tree: %p, count: %u\n",
             btree, btree->count );

    btree_traverse ( btree, 0 );
}


//
//  Dump both of the memory management B-trees.
//
void dumpSM ( void )
{
    //
    //  If the system has not finished initializing yet, just ignore this
    //  call.
    //
    if ( ! smControl->systemInitialized )
    {
        return;
    }
    dumpFreeBySize();
    dumpFreeByOffset();
    dumpAllSignals( 10 );
}


/*!----------------------------------------------------------------------------

    f i n d S i g n a l L i s t

	@brief Find the the signal list for a domain and key value.

	This function will search the signals btree for a match to the domain and
    id value supplied.  If a match is found, the address of the signal list
    control block will be returned to the caller.  If no match is found, a
    null pointer will be returned.

	@param[in] - domain - The domain of the signal list to find
	@param[in] - key - The id of the signal list to find

	@return If successful, the address of the signal list control block
            If not found, a NULL

-----------------------------------------------------------------------------*/
signalList_t* findSignalList ( domain_t domain, vsiKey_t key )
{
    signalList_t signalList;

    signalList.domain = domain;
    signalList.key    = key;

    return btree_search ( &smControl->signals, &signalList );
}


/*!-----------------------------------------------------------------------

    s m _ i n s e r t

    @brief Insert a new message into the message buffer.

    This function will find the appropriate signal list for the given key and
    domain and insert the caller's data into the message list.

    The "key" is an arbitrary integer that uniquely identifies the message
    type within the domain of the message.  Domains are an identifier that
    specifies a class of messages.  For instance, CAN messages can all be
    considered to be messages in the CAN domain.  Within that domain, the
    message ID field uniquely identifies the type of message.

    The "body" pointer supplied by the caller must point to the beginning of
    the data that makes up the message we want to store.  The message size
    specifies the number of bytes that will be read from the user's pointer
    and copied into the shared memory segment.

    @param[in] domain - The domain associated with this message.
    @param[in] key - The key value associated with this message.
    @param[in] newMessageSize - The size of the new message in bytes.
    @param[in] body - The address of the body of the new message.

    @return 0 if successful
		    Otherwise the error code

------------------------------------------------------------------------*/
int sm_insert ( domain_t domain, vsiKey_t key, unsigned long newMessageSize,
                void* body )
{
    int status = 0;

    LOG ( "Inserting domain[%lu] key[%lu]\n", domain, key );

    HX_DUMP ( body, newMessageSize, "New Message" );

    //
    //  Go find the signal list control block for this domain and key.
    //
    signalList_t* signalList = findSignalList ( domain, key );

    //
    //  If we didn't find this signal list control block then this is the
    //  first time this domain/id have been seen so we need to create a new
    //  signal list control block for it.
    //
    if ( signalList == NULL )
    {
        //
        //  Go allocate a new signal list control block in the shared memory
        //  segment.
        //
        signalList = sm_malloc ( SIGNAL_LIST_SIZE );

        //
        //  If the allocation failed, we have exceeded the amount of memory in
        //  the shared memory segment.  This is a fatal condition so let the
        //  user know and quit!
        //
        if ( signalList == NULL )
        {
            printf ( "Error: Unable to allocate a new signal list - "
                     "Shared memory segment is full!\n" );
            return ENOMEM;
        }
        //
        //  Initialize and populate the new signal list control block.
        //
        signalList->domain                = domain;
        signalList->key                   = key;
        signalList->currentSignalCount    = 0;
        signalList->totalSignalSize       = 0;
        signalList->head                  = END_OF_LIST_MARKER;
        signalList->tail                  = END_OF_LIST_MARKER;

        //
        //  Initialize the signal list semaphore.
        //
        status = pthread_mutex_init ( &signalList->semaphore.mutex,
                                      &smControl->masterMutexAttributes );
        if ( status != 0 )
        {
            printf ( "Unable to initialize signal list mutex - errno: %u[%m].\n",
                     status );
            sm_free ( signalList );
            return errno;
        }
        status = pthread_cond_init ( &signalList->semaphore.conditionVariable,
                                     &smControl->masterCvAttributes );
        if ( status != 0 )
        {
            printf ( "Unable to initialize signal list condition variable - "
                     "errno: %u[%m].\n", status );
            sm_free ( signalList );
            return errno;
        }
        //
        //  Initialize the semaphore counters for this signal list control block.
        //
        signalList->semaphore.messageCount = 0;
        signalList->semaphore.waiterCount  = 0;

        btree_insert ( &smControl->signals, signalList );
    }
    //
    //  Acquire the lock on this signal list.
    //
    //  Note that this call will hang if someone else is currently using this
    //  signal list.  It will return once the lock is acquired and it is safe
    //  to manipulate the signal list.
    //
    SM_LOCK;

    //
    //  Now we need to create a new entry for this message list so compute the
    //  size that we will need and allocate that much memory in the shared
    //  memory segment.
    //
    int signalDataSize = newMessageSize + SIGNAL_DATA_HEADER_SIZE;
    signalData_t* signalData = sm_malloc ( signalDataSize );
    if ( signalData == NULL )
    {
        printf ( "Error: Unable to allocate a new signal list - "
                 "Shared memory segment is full!\n" );
        status = ENOMEM;
        goto insertExit;
    }
    //
    //  Initialize all of the fields in the message header of the new message
    //  that we are inserting.  We will be inserting this new message at the
    //  end of the message list which is where the "tail" pointer is pointing.
    //
    //  If the message list is currently empty (because the head pointer is
    //  NULL), make the head pointer point to our new message.
    //
    offset_t newMessageOffset = toOffset ( signalData );
    if ( signalList->head == END_OF_LIST_MARKER )
    {
        signalList->head = newMessageOffset;
    }
    //
    //  If the tail pointer is not NULL, make the current tail message point
    //  to our new message.
    //
    if ( signalList->tail != END_OF_LIST_MARKER )
    {
        ((signalData_t*)toAddress(signalList->tail))->nextMessageOffset =
            newMessageOffset;
    }
    //
    //  Now make the tail pointer point to our new message.
    //
    signalList->tail = newMessageOffset;

    //
    //  Make the "next" pointer in our new message indicate the "end" of the
    //  list.
    //
    signalData->nextMessageOffset = END_OF_LIST_MARKER;

    //
    //  Copy the message body into the message list.
    //
    //  TODO: Validate that the message size is "reasonable"!
    //
    signalData->messageSize = newMessageSize;
    memcpy ( (void*)(&signalData->data), body, newMessageSize );

    //
    //  Increment the message count and message sequence numbers.
    //
    ++signalList->currentSignalCount;
    signalList->totalSignalSize += newMessageSize;

    //
    //  Increment the message count in the semaphore.
    //
    ++signalList->semaphore.messageCount;

    //
    //  Post to the semaphore to reflect the message we just inserted into the
    //  message list.
    //
    LOG ( "%'lu  Before Insert/semaphore post:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    semaphorePost ( &signalList->semaphore );

    LOG ( "%'lu  After Insert/semaphore post:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

insertExit:

    //
    //  Give up the signal list lock.
    //
    SM_UNLOCK

    //
    //  Return to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ r e m o v e S i g n a l

	@brief Remove the oldest signal from the signal list.

    This function will remove the signal at the beginning of the specified
    signal list.

	WARNING: This function assumes that the signal list mutex has been
	acquired before this function is called.

	@param[in] signalList - The address of the signal list to operate on.

	@return 0 - Message successfully removed.

------------------------------------------------------------------------*/
int sm_removeSignal ( signalList_p signalList )
{
	signalData_t* signal;

	LOG ( "%'lu  Removing signal...\n", getIntervalTime() );

    //
    //  If this signal list is empty, just return without doing anything.
    //
    if ( signalList->head == END_OF_LIST_MARKER )
    {
        return 0;
    }
	//
	//	Grab the first signal on the given signalList control block.
	//
	signal = toAddress ( signalList->head );

    //
    //	If there is only one message in the current list then the head and
    //	tail offsets will be the same and we should just set both of them to
    //	the "empty" list state.
    //
    if ( signalList->currentSignalCount == 1 )
    {
        signalList->head = END_OF_LIST_MARKER;
        signalList->tail = END_OF_LIST_MARKER;
    }
    //
    //	Otherwise, just make the head offset be the offset of the next signal
    //	after the one we are removing.
    //
    else
    {
        signalList->head = signal->nextMessageOffset;
    }
	//
	//	Decrement the count of the number of signals in this list and the
    //	total amount of space occupied by those signals.
	//
	--signalList->currentSignalCount;
    signalList->totalSignalSize -= signal->messageSize;

	//
	//	Decrement the count of the number of signals in the list within the
	//	semaphore.
	//
	--signalList->semaphore.messageCount;

    //
    //  Free up the shared memory occupied by this signal data structure.
    //
    sm_free ( signal );

	//
	//	Return a good completion code to the caller.
	//
	return 0;
}


/*!-----------------------------------------------------------------------

    s m _ f e t c h

    @brief Retrieve a signal from the signal buffer.

    This function will attempt to retrieve a signal with the specified key and
    domain from the shared memory signal buffer.  The signal that was found
    (and returned to the caller) will be removed from the signal list.  If
    there are no signal that match the user's request, this function will hang
    (on a semaphore) until an appropriate signal is available before returning
    to the caller.

    If the signal list is not empty, the head of the list will be returned to
    the caller.  This is also the oldest signal in the list since the signals
    are added at the end of the list.

    When calling this function, the bodySize should be the size of the data
    buffer supplied as the "body" argument.  When we copy the data from the
    shared memory segment to the "body" buffer, the smaller of either the body
    buffer size or the number of bytes of data in the signal found will be
    copied into the body buffer.

    Note that since this function does not alter the structure of the signal
    list we don't need to lock the signal list semaphore during the processing
    (except when we wait on an empty list).

    @param[in]  domain - The domain value of the signal to be removed.
    @param[in]  key - The key value of the signal to be removed.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the signal data.
    @param[in]  wait - If true, wait for data if domain/key is not found.

    @return 0 if successful.
            ENODATA - If waitForData == false and domain/key is not found.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_fetch ( domain_t domain, vsiKey_t key, unsigned long* bodySize,
               void* body, bool wait )
{
    signalData_t* signal;
    int           transferSize;
    int           status = 0;

    LOG ( "Fetching signal domain[%lu], key[%lu], wait[%d]\n", domain, key,
          wait );
    //
    //  Go find the signal list control block for this domain and key.
    //
    signalList_t* signalList = findSignalList ( domain, key );

    //
    //  If we didn't find this signal list control block then we need to just
    //  return an error code to the caller.
    //
    //  TODO: What should we do here if the user asked us to wait for the
    //  signal but this signal isn't defined yet?  Should we create the signal
    //  list block and then wait on it?
    //
    if ( signalList == NULL )
    {
        return ENODATA;
    }
    //
    //  If this signal list was found but it is empty and we don't want to
    //  wait for the data to arrive then just return the "no data" error code
    //  to the caller and quit.
    //
    if ( signalList->semaphore.messageCount == 0 && !wait )
    {
        return ENODATA;
    }
    //
    //  Go wait if there are no signals in the signal list to be read.
    //
    //  Note that we will increment the "waiter" count during the time that we
    //  are potentially waiting on the semaphore and decrement it again when
    //  we come back from the wait.  This is used to tell the "fetch" logic
    //  when it is OK to delete the signal that is being waited on.  If more
    //  than one process is waiting on this signal then we want to give the
    //  signal to all of the processes that are waiting before we delete the
    //  signal from the signal list.
    //
    ++signalList->semaphore.waiterCount;

    LOG ( "%'lu  Before Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    semaphoreWait ( &signalList->semaphore );

    --signalList->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    //
    //  Get the actual memory pointer to the current signal.
    //
    signal = toAddress ( signalList->head );

    //
    //  Copy the signal body into the caller supplied buffer.
    //
    //  Note that we will copy the SMALLER of either the signal size supplied
    //  by the caller or the actual data size in the signal that was found.
    //
    transferSize = *bodySize <= signal->messageSize
                   ? *bodySize
                   : signal->messageSize;

    memcpy ( body, signal->data, transferSize );

    //
    //  Return the number of bytes that were copied to the caller.
    //
    *bodySize = transferSize;

    //
    //  If no one else is now waiting for this signal, go remove this signal
    //  from the signal list.
    //
    if ( signalList->semaphore.waiterCount <= 0 )
    {
        status = sm_removeSignal ( signalList );
    }
    LOG ( "%'lu  At the end of Fetch - waiterCount: %d\n", getIntervalTime(),
          signalList->semaphore.waiterCount );

    SEM_DUMP ( &signalList->semaphore );

    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    s m _ f e t c h _ n e w e s t

    @brief Retrieve the newest signal from the signal list.

    This function will attempt to retrieve the newest signal with the
    specified key and domain from the shared memory signal list.  The signal
    that is found (and returned to the caller) will NOT be removed from the
    signal list.  If there are no signals that match the user's request, this
    function will hang (on a semaphore) until an appropriate signal is
    available before returning to the caller depending on the setting of the
    "wait" input parameter.

    When calling this function, the bodySize should be the size of the data
    buffer supplied as the "body" argument.  When we copy the data from the
    shared memory segment to the "body" buffer, the smaller of either the body
    buffer size or the number of bytes of data in the signal found will be
    copied into the body buffer.  The actual number of data bytes copied will
    be returned to the caller in the bodySize variable.

    Note that since this function does not alter the structure of the signal
    list we don't need to lock the signal list semaphore during the processing
    (except when we wait on an empty list).

    @param[in]  domain - The domain value of the signal to be removed.
    @param[in]  key - The key value of the signal to be removed.
    @param[in/out] bodySize - The address of the body buffer size.
    @param[out] body - The address of where to put the signal data.
    @param[in]  wait - If true, wait for data if domain/key is not found.

    @return 0 if successful.
            ENODATA - If waitForData == true and domain/key is not found.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_fetch_newest ( domain_t domain, vsiKey_t key, unsigned long* bodySize,
                      void* body, bool wait )
{
    //
    //  Define the local signal offset and pointer variables.
    //
    signalList_t* signalList;
    signalData_t* signal;
    int           transferSize = 0;
    int           status = 0;

    LOG ( "Fetching newest signal domain[%lu], key[%lu], wait[%d]\n", domain,
          key, wait );
    //
    //  Go find the signal list control block for this domain and key.
    //
    signalList = findSignalList ( domain, key );

    //
    //  If this signal list is empty and we don't want to wait for the data to
    //  arrive then just return the "no data" error code to the caller and
    //  quit.
    //
    if ( signalList->semaphore.messageCount == 0 && !wait )
    {
        LOG ( "Signal list is empty - Returning error code ENODATA\n" );
        return ENODATA;
    }
    //
    //  Go wait if there are no messages in the signal list to be read.
    //
    //  Note that we will increment the "waiter" count during the time that we
    //  are potentially waiting on the semaphore and decrement it again when
    //  we come back from the wait.  This is used to tell the "fetch" logic
    //  when it is OK to delete the signal that is being waited on.  If more
    //  than one process is waiting on this signal then we want to give the
    //  signal to all of the processes that are waiting before we delete the
    //  signal from the signal list.
    //
    ++signalList->semaphore.waiterCount;

    LOG ( "%'lu  Before Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    semaphoreWait ( &signalList->semaphore );

    --signalList->semaphore.waiterCount;

    LOG ( "%'lu  After Fetch/semaphore wait:\n", getIntervalTime() );
    SEM_DUMP ( &signalList->semaphore );

    //
    //  If we get here, there is at least one signal in this signal list.  At
    //  that point, we can take a look at the last signal in the bucket.
    //
    signal = toAddress ( signalList->tail );

    //
    //  Copy the signal body into the caller supplied buffer.
    //
    //  Note that we will copy the SMALLER of either the signal size
    //  supplied by the caller or the actual data size in the signal
    //  that was found.
    //
    transferSize = *bodySize <= signal->messageSize
                   ? *bodySize : signal->messageSize;

    memcpy ( body, signal->data, transferSize );

    //
    //  Return the number of bytes that were copied to the caller.
    //
    *bodySize = transferSize;

    //
    //  Return to the caller with a good completion code.
    //
    //  Note: Sorry for the "goto" but it's the easiest way to ensure that
    //  this function only has one exit path.  The real issue is that the
    //  pthread cleanup push & pop functions HAVE to be paired in the same
    //  compilation context... A REALLY STUPID IMPLEMENTATION!!
    //  (commentary by DWM).
    //
    LOG ( "%'lu  At the end of Fetch - waiterCount: %d\n", getIntervalTime(),
          signalList->semaphore.waiterCount );

    SEM_DUMP ( &signalList->semaphore );

    //
    //  Return the completion code to the caller.
    //
    LOG ( "Returning error code %d\n", status );

    return status;
}


/*!-----------------------------------------------------------------------

    s m _ f l u s h _ s i g n a l

    @brief Flush all specified signals from the data store.

    This function will find all signals in the data store with the specified
    domain and key value and remove them from the data store.

    @param[in]  domain - The domain value of the message to be removed.
    @param[in]  key - The key value of the message to be removed.

    @return 0 if successful.
            any other value is an errno value.

------------------------------------------------------------------------*/
int sm_flush_signal ( domain_t domain, vsiKey_t key )
{
    //
    //  Define the local signal offset and pointer variables.
    //
    signalData_t* signal;
    offset_t      signalOffset;
    int           status = 0;

    LOG ( "Flushing signal domain[%lu], key[%lu]\n", domain, key );

    //
    //  Go find the signal list control block for this domain and key.
    //
    signalList_t* signalList = findSignalList ( domain, key );

    //
    //  If this signal list doesn't exist or is empty then we don't need to do
    //  anything.  In this case, just return a good completion code.
    //
    if ( signalList == NULL || signalList->semaphore.messageCount == 0 )
    {
        return 0;
    }
    //
    //  Acquire the lock on the shared memory segment.
    //
    //  Note that this call will hang if someone else is currently using the
    //  shared memory segment.  It will return once the lock is acquired and it
    //  is safe to manipulate the shared memory data.
    //
    SM_LOCK;

    //
    //  Get the head of the signal list.
    //
    signalOffset = signalList->head;

    //
    //  Repeat until we reach the end of the signal list in this bucket.
    //
    while ( signalOffset != END_OF_LIST_MARKER )
    {
        //
        //  Get the actual memory pointer to the current signal.
        //
        signal =  toAddress ( signalOffset );

        //
        //  Save the offset of the next signal before we destroy the current
        //  signal.
        //
        signalOffset = signal->nextMessageOffset;

        //
        //  Go free the memory occupied by this signal record.
        //
        sm_free ( signal );

        //
        //  Increment our signal offsets to the next signal in the list.
        //
        signal = toAddress ( signalOffset );
    }
    signalList->head               = END_OF_LIST_MARKER;
    signalList->tail               = END_OF_LIST_MARKER;
    signalList->currentSignalCount = 0;
    signalList->totalSignalSize    = 0;

    //
    //  If anyone is waiting for this signal, go release them to search the
    //  signal list again.
    //
    while ( signalList->semaphore.waiterCount > 0 )
    {
        semaphorePost ( &signalList->semaphore );
    }
    //
    //  Give up the shared memory block lock.
    //
    SM_UNLOCK;

    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!----------------------------------------------------------------------------

    S i g n a l   D e b u g   F u n c t i o n s

	The following functions are designed for debugging purposes and will
    normally not be included in production builds.

-----------------------------------------------------------------------------*/
#if 0
#ifdef VSI_DEBUG
//
//  Print one signal's data in a neat format.
//
void SM_signalDataPrint ( char* leader, signalData_t* signal )
{
    LOG ( "%sNext: %lx, Size: %lu\n", leader, signal->nextMessageOffset,
          signal->messageSize );

    HexDump ( (const char*)&signal->data, signal->messageSize, "  ", 0 );

    return;
}


//
//  Print out the information in a signalList control block in a nice format.
//
void signalListPrint ( char* leader, void* recordPtr )
{
    dumpSignalList ( recordPtr, 4 );
    signalList_t* sl = recordPtr;

    LOG ( "\n  Signal List [%p:%lu]\n", sl, toOffset ( sl ) );
    LOG ( "    Domain.....: %lu\n",     sl->domain );
    LOG ( "    Key........: %lu\n",     sl->key );
    LOG ( "    Head.......: %lu\n",     sl->head );
    LOG ( "    Tail.......: %lu\n",     sl->tail );
    LOG ( "    Signal Cnt.: %lu\n",     sl->currentSignalCount );
    LOG ( "    Tot Sig Sz.: %lu\n",     sl->totalSignalSize );

    //
    //  Run down the list of signals in this signal list and print each one of
    //  them.
    //
    signalData_t* signal = toAddress ( sl->head );
    do
    {
        dumpSignals ( sl, 0 );

        signal = toAddress ( signal->nextMessageOffset );

    }   while ( signal->nextMessageOffset != END_OF_LIST_MARKER );
    return;
}
#endif
#endif


//
//  Define a default traverse callback function that just prints out
//  signalList control block info.
//
void signalListTraverse ( void* data )
{
    // SM_signalListPrint ( "   ", data );
    dumpSignalList ( data, 0 );
}


/*! @} */

// vim:filetype=c:syntax=c
