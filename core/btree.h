/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

	@file      btree.h

	This file defines the data structures and configuration data for the
    shareed memory btree code.

-----------------------------------------------------------------------------*/

#ifndef _BTREE_SM_H_
#define _BTREE_SM_H_

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <stddef.h>
#include <stdbool.h>


//
//  The following defines will enable some debugging features if they are
//  defined.  If these features are not desired, either comment out the
//  following lines or undefine the symbols.
//
//  BTREE_DEBUG: If defined, will cause the btree "print" functions to be
//               compiled and linked.
//
//  BTREE_VERBOSE: If defined, will cause the verbose "print" statements to be
//               compiled and the hex dump function enabled.
//
//  BTREE_VALIDATE: If defined, will cause the btree test code to run btree
//               validation functions during the tests to ensure that the
//               integrity of the btree has been maintained.
//
#define BTREE_DEBUG
#undef BTREE_VERBOSE
#undef BTREE_VALIDATE

#ifdef BTREE_DEBUG
#  define BLOG        printf
#  define PRINT_BTREE btree_print
#  define PRINT_DATA  printFunction
#  define PRINT_NODE(tree,node)  print_single_node(tree,node,NULL)
#  ifdef BTREE_VERBOSE
#    define DUMP(data,length) HexDump((const char*)data,length,"",0)
#    define DUMP_T(data,length,title) HexDump((const char*)data,length,title,0)
#    define DUMP_L(data,length,lead) HexDump((const char*)data,length,"",lead)
#    define DUMP_TL(data,length,title,lead) HexDump((const char*)data,length,title,lead)
#  else
#    define DUMP(...)
#    define DUMP_T(...)
#    define DUMP_L(...)
#    define DUMP_TL(...)
#  endif
#else
#  define BLOG(...)
#  define PRINT_BTREE(...)
#  define PRINT_DATA(...)
#  define PRINT_NODE(...)
#  define DUMP(...)
#  define DUMP_T(...)
#  define DUMP_L(...)
#  define DUMP_TL(...)
#endif

#ifdef BTREE_VALIDATE
#  define VALIDATE_BTREE validateBtree
#  define FIND_TEST   findTest
#  define RFIND_TEST  rFindTest
#else
#  define VALIDATE_BTREE(...)
#  define FIND_TEST(...)
#  define RFIND_TEST(...)
#endif

/*! @{ */

//
//    S h a r e d   M e m o r y   B t r e e   L i b r a r y
//
//
//  Much of the code (the algorithms at least) and terminology used in this
//  code was derived from the chapter on B-trees in "Introduction to
//  Algorithms" by Thomas H. Cormen.
//
//  The following library is an implementation of the btree algorithm which is
//  designed to be used in a shared memory environment in which pointers to
//  data cannot be used.
//
//  In a shared memory environment, a segment of memory is given a name by
//  storing it persistently in a disk file and multiple tasks and/or threads
//  can simultaneously open that shared memory segment without having problems
//  caused by the fact that the beginning of the share memory segment can be
//  located at a different address in memory for all of them.
//
//  To make our life easier in the btree library, we will modify the btree
//  algorithm a bit so that each of the tree nodes is the same size.  In this
//  implementation, I have chosen to use the size of a memory page because
//  this is the unit of allocation for shared memory segments.  A shared
//  memory segment is always a integral number of memory pages in size,
//  regardless of the size the user actually specified.  If the user specifies
//  a size that is not an integral number of memory pages, the shared memory
//  allocator will round up the requested size to be an integral number of
//  memory pages.
//
//  Using the physical memory page size also facilitates dynamically resizing
//  the share memory segment.  To make the shared memory segment larger
//  involves just mucking around with some of the memory management unit
//  tables and no data has to be copied in order to make that happen.  The new
//  pages are just tacked onto the end of the existing segment in the MMU and
//  everyone is happy.  This makes resizing the shared memory segment very
//  fast and easy.
//
//  Since pointers cannot be stored anywhere, we get around this limitation by
//  storing offsets from the beginning of the shared memory segment.  Then the
//  code can easily calculate the actual logical address of a component by
//  simply taking the offset value and adding it to the base address of the
//  shared memory segment in it's particular address space.
//
//  The size of a system memory page can be gotten with a call to
//  sysconf(_SC_PAGE_SIZE).  Rather than hard coding the size, we will fetch
//  it at initialization time and work with that value to compute the rest of
//  the btree parameters that will be required.  The places where one would
//  normally expect a pointer (such as the child nodes of a given node) will
//  be given as an offset_t type which is an unsigned long value.
//
//  Cormen defines a value that he calls "t" in his equations and refers to as
//  the "minimum degree" of the btree which determines the size of the btree
//  nodes as follows:
//
//    MAX and MIN are the maximum and minumum number of keys that can be
//    contained in a node and t is Cormen's minimum degree variable.  Then:
//
//    MAX = 2t - 1
//
//    and the maximum number of child references is then 2t.
//
//    Then the size of a node is given by:
//
//    nodeSize = nodeHeaderSize + dataOffsetSize*(2*t-1) + linkOffsetSize*(2*t)
//
//  In our case, both the dataOffsetSize and the linkOffsetSize are the same,
//  which is the size of a "void*" type or 8 bytes on our 64-bit Linux
//  systems.  In the initial iteration of this code, the nodeHeaderSize is 32
//  bytes.  Note that there is always one more child link than there are data
//  records in a node.  That's an artifact of how the btree algorithm
//  operates.
//
//  So now to make things a bit more concrete, lets assume that our memory
//  page size is 4K (which it is on our development systems).
//
//  Substituting all the numbers that we know...
//
//    4096 = 32 + 8 * (2 * t - 1) + 8 * (2 * t)
//    4096 = 32 + 16 * t - 8      + 16 * t
//    4096 = 32 + 32 * t - 8
//    4096 = 24 + 32 * t
//
//  Solving this equation for t gives:
//
//    t = ( 4096 - 24 ) / 32
//    t = 127.25
//
//  Ignoring the fractional part which will represent some unused space at the
//  end of each node, we get:
//
//    t = 127
//    MAX = 253
//    MIN = 126
//    maxChildReferences = 2t = 254
//
//  Note that this calculation needs to take into account any alignment that
//  might take place inside the structure.  The beginning of the structure
//  will always be on a memory page boundary so no alignment takes place
//  there.
//
//  Note also that with this calculation there is 16 bytes of space left
//  unused in each btree node:
//
//    ( MAX + maxChildrenReferences ) * sizeof(void*)
//    ( 253 + 254 ) * 8 = 4080
//
//  It would seem possible to add one additional key and pointer to each node
//  to completely fill each node but there is an algorithmic constraint that
//  is not explicitly explained in Cormen's book that each node must contain
//  and odd number of keys in order for the node coalescing logic to work
//  properly.  Adding the extra key and pointer would violate that constraint
//  so we'll have to live with a bit of unused space in each node.
//
//  The original code for this library contained a variable named "order"
//  which is actually Cormen's "t" minimum degree variable.  I've renamed it
//  in this implementation to "minDegree" to eliminate some confusion,
//  especially since some other authors refer to "order" with a different
//  meaning altogether.
//

//
//  Define the btree "offset" type that will be used in place of all of the
//  pointer types normally found in a tree.
//
typedef unsigned long offset_t;


//
//  Define the callback function types used by the btree code.
//
typedef void (*traverseFunc) ( char*, void* );

typedef void (*printFunc)    ( char*, void* );


//
//  Define the flags for the btree "type".
//
#define TYPE_USER   ( 0 )
#define TYPE_SYSTEM ( 1 )


//
//  Define the sizes of various things that are often used.  They are:
//
//  sO - size of an offset data type
//  sN - size of the btree node header structure
//  sM - size of a shared memory block control structure
//  sP - size of a generic pointer type
//  sD - size of the dataRecords area of a btree node
//  sC - size of the children pointer area of a btree node
//
//  Note that all sizes are in bytes.  The sC value is one offset larger than
//  the sD value because there is always one more pointer than there are data
//  records in each tree node.
//
#define sO ( sizeof ( offset_t ) )
#define sN ( sizeof ( bt_node_t ) )
#define sM ( sizeof ( memoryChunk_t ) )
#define sP ( sizeof ( void* ) )
#define sD ( sO * btree->max )
#define sC ( sD + sO )


//
//  Define the structure of a single btree node.
//
//  Note that the offsets of the dataRecords and children are actually
//  calculated at runtime and stored here for easy use.
//
typedef struct bt_node_t
{
    offset_t     next;        // Pointer used for linked list traversal
    offset_t     parent;      // Pointer to the parent of this node
    unsigned int keysInUse;   // Number of keys currently defined
    unsigned int level;       // Level of this node in the btree
    offset_t     dataRecords; // Offset of the Array of data record offsets
    offset_t     children;    // Offset of the Array of link offsets to children

}   bt_node_t;


//
//  Define the btree definition structure.  In this incarnation of the btree
//  algorithm, the keys are offsets to a user specified data structure
//  allocated in the shared memory segment.  The keys and values are then
//  contained in that user data structure and the btree supplied comparison
//  function is called to compare the keys in two instances of these records.
//
typedef struct btree_t
{
    unsigned int maxRecCnt;  // The max number of records per node in this btree
    unsigned int nodeSize;   // The size of a B-tree node in bytes
    unsigned int type;       // The type of B-tree this is (system vs. user)
    unsigned int minDegree;  // The minumum degree, i.e. "t", value
    unsigned int min;        // The minimum number of records per node
    unsigned int max;        // The maximum number of records per node
    unsigned int count;      // The total number of records in the btree
    unsigned int numKeys;    // The number of key fields to be compared
    char         keys[4];    // The array of record indices to be compared
    offset_t     root;       // The offset of the root node of the btree

}   btree_t;


//
//  Define the btree iterator control structure.  This structure is used to
//  keep track of positions within the btree and is used by the iterator set
//  of functions to enable the "get_next" and "get_previous" functions.
//
//  Note that the iterator objected contains memory pointers instead of
//  offsets.  This is because iterators are transient objects and do not
//  persist between processes.  They are process local objects.
//
typedef struct btree_iterator_t
{
    btree_t*     btree;
    void*        key;
    bt_node_t*   node;
    unsigned int index;

}   btree_iterator_t;

typedef btree_iterator_t* btree_iter;


//
//  Define the public API to the btree library.
//
extern void     btree_initialize ( btree_t* btree,
                                   unsigned int maxRecordsPerNode,
                                   unsigned int numberOfKeys,
                                   char key1index,
                                   char key2index,
                                   char key3index,
                                   char key4index );

extern btree_t* btree_create   ( unsigned int maxRecordsPerNode,
                                 unsigned int numberOfKeys,
                                 char key1index,
                                 char key2index,
                                 char key3index,
                                 char key4index );

extern btree_t* btree_create_sys ( btree_t* btree,
                                   unsigned int maxRecordsPerNode,
                                   unsigned int numberOfKeys,
                                   char key1index,
                                   char key2index,
                                   char key3index,
                                   char key4index );

extern void     btree_create_in_place ( btree_t* btree,
                                        unsigned int maxRecordsPerNode,
                                        unsigned int numberOfKeys,
                                        char key1index,
                                        char key2index,
                                        char key3index,
                                        char key4index );

extern void     btree_destroy  ( btree_t* btree );

extern int      btree_insert   ( btree_t* btree, void* data );

extern int      btree_delete   ( btree_t* btree, void* key );

extern void*    btree_get_min  ( btree_t* btree );

extern void*    btree_get_max  ( btree_t* btree );

extern void*    btree_search   ( btree_t* btree, void* key );

extern void     btree_traverse ( btree_t* btree, traverseFunc traverseCB );

extern void     btree_print    ( btree_t* btree, printFunc printCB );

//
//  The following functions have default implementations but the user should
//  implement them if he does not want the default supplied function to be
//  called.  The user should define these as the format of their "userData" is
//  probably not the same as that used in the default implementation.
//
//  If the user defines his own version of these functions, he can supply them
//  directly to the btree member functions that require them as arguments.
//  Doing so will allow the user to use any function name he wishes when he
//  implements this functionality.
//
// extern void printFunction    ( char* leader, void* userData );
// extern void traverseFunction ( void* userData );

//
//  Define the btree iterator functions.
//
//  To iterate through a range of values in the btree the user needs to
//  execute one of the "find" functions to position an iterator which is
//  returned by the "find" function and then iterate from there by calling
//  either the btree_iter_next or btree_iter_previous functions until the
//  position in the btree no longer satisfies the user's terminating
//  conditions.
//
//  The user may iterate forward (to increasing keys) or backward by using the
//  either the btree_iter_next or btree_iter_previous functions.
//
//     Example usage (assuming the btree is populated with "userData" records):
//
//         userData record = { 12, "" };
//
//         btree_iter iter = btree_find ( &record );
//
//         while ( ! btree_iter_at_end ( iter ) )
//         {
//             userData* returnedData = btree_iter_data ( iter );
//             ...
//             btree_iter_next ( iter );
//         }
//         btree_iter_cleanup ( iter );
//
//     The above example will find all of the records in the btree beginning with
//     { 12, "" } to the end of the btree.
//
//  Note that this iterator definition is designed to operate similarly to the
//  C++ Standard Template Library's container iterators.  There are obviously
//  some differences due to the language differences but it's pretty close.
//

//
//  The btree_find function is similar to the btree_search function except
//  that it will find the first key that is greater than or equal to the
//  specified value (rather than just the key that is equal to the specified
//  value).  This function is designed to be used for forward iterations using
//  the iter->next function to initialize the user's iterator to the first key
//  location.
//
extern btree_iter btree_find ( btree_t* btree, void* key );

//
//  The btree_rfind function is similar to the btree_search function except
//  that it will find the last key that is less than or equal to the specified
//  value (rather than just the key that is equal to the specified value).
//  This function is designed to be used for reverse iterations using the
//  iter->previous function to initialize the user's iterator to the last key
//  location.
//
extern btree_iter btree_rfind ( btree_t* btree, void* key );

//
//  Position the specified iterator to the first record in the btree.
//
extern btree_iter btree_iter_begin ( btree_t* btree );

//
//  Position the specified itrator to the "end" position in the btree (which
//  is past the last record of the btree.
//
extern btree_iter btree_iter_end ( btree_t* btree );

//
//  Position the specified iterator to the next higher key value in the btree
//  from the current position.
//
extern void btree_iter_next ( btree_iter iter );

//
//  Position the specified iterator to the next lower key value in the btree
//  from the current position.
//
extern void btree_iter_previous ( btree_iter iter );

//
//  Compare the 2 iterators and return their relative values as determined by
//  the comparison function of the current btree.
//
extern int btree_iter_cmp ( btree_iter iter1, btree_iter iter2 );

//
//  Test to see if the given iterator is at the end of the btree.
//
extern bool btree_iter_at_end ( btree_iter iter );

//
//  Return the pointer to the user data structure that is stored in the btree
//  at the current iterator position.
//
extern void* btree_iter_data ( btree_iter iter );

//
//  Cleanup the specified iterator and release any resources being used by
//  this iterator.  This function MUST be called when the user is finished
//  using an iterator or resources (such as memory blocks) will be leaked to
//  the system.  Once this function has been called with an iterator, that
//  iterator may not be used again for any of the btree functions unless it
//  has subsequently been reinitialized using one of the iterator
//  initialization functions defined here.
//
extern void btree_iter_cleanup ( btree_iter iter );


//
//  Define some things that are only used when debugging the btree code.
//
#ifdef BTREE_DEBUG

extern void print_subtree ( btree_t* btree, bt_node_t* node, printFunc printCB );

#endif

#ifdef BTREE_VALIDATE

extern void validateBtree ( btree_t* btree, int maxSize, bool reportMissing,
                            int firstValid, int lastValid );

extern void findTest ( btree_t* btree, int domain, int signal, bool validate );

extern void rFindTest ( btree_t* btree, int domain, int signal, bool validate );

#endif      // end of #ifdef BTREE_VALIDATE

#endif      // end of #ifndef _BTREE_SM_H_


/*! @} */

// vim:filetype=c:syntax=c
