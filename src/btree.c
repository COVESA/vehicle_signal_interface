/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

    @file btree.c

    This file implements the shared memory capable btree data structure code.

    TODO: Change the iterator api to allocate iterators only when needed,
    otherwise, reuse an existing iterator.

    TODO: Update the headers for the new keyDef stuff!

    TODO: Check for duplicate keys on insert.

    Much of the code (the algorithms at least) and terminology used in this
    code was derived from the chapter on B-trees in "Introduction to
    Algorithms" by Thomas H. Cormen.

-----------------------------------------------------------------------------*/

/*! @{ */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "btree.h"
#include "sharedMemory.h"
#include "utils.h"

//
//  If the following define is changed to an "#undef", the mutex locking code
//  for the B-tree will not be generated.  This can speed up the code a small
//  amount in cases where this functionality is being used by a single thread
//  in a single task only.
//
#define BTREE_LOCKS_ENABLE


//
//  Define an enumerated type that we can use to specify "left" and "right".
//
typedef enum
{
    left  = -1,
    right =  1

}   position_t;


//
//  Define the btree position definition structure.  This structure contains
//  all the information needed to uniquely define a position within the tree.
//
typedef struct
{
    bt_node_t*   node;
    unsigned int index;

}   nodePosition;


//
//  Define the local functions that are not exposed in the API.
//
static bt_node_t* allocate_btree_node ( btree_t* btree );

static void init_node_header ( btree_t* btree,
                               bt_node_t* node );

static void btree_insert_nonfull ( btree_t* btree,
                                   bt_node_t* parent_node,
                                   void* data );

static void free_btree_node ( btree_t* btree,
                              bt_node_t* node );

static nodePosition get_btree_node ( btree_t* btree,
                                     void* key );

static int btree_delete_subtree ( btree_t* btree,
                                  bt_node_t* subtree,
                                  void* key );

static int delete_key_from_node ( btree_t* btree,
                                  nodePosition* nodePosition );

static void move_key ( btree_t* btree,
                       bt_node_t* node,
                       unsigned int index,
                       position_t pos );

static nodePosition get_max_key_pos ( btree_t* btree,
                                      bt_node_t* subtree );

static nodePosition get_min_key_pos ( btree_t* btree,
                                      bt_node_t* subtree );

static bt_node_t* merge_siblings ( btree_t* btree,
                                   bt_node_t* parent,
                                   unsigned int index );

static void btree_traverse_node ( btree_t*     btree,
                                  bt_node_t*   subtree,
                                  traverseFunc traverseFunction );

static int btree_compare_function ( btree_t* btree,
                                    void* data1,
                                    void* data2 );

static void btree_print_header ( btree_t* btree );

static void btree_print_function ( btree_t* btree,
                                   void* data );

static void printFunction ( char* leader,
                            void* data );

static void print_single_node ( btree_t* btree,
                                bt_node_t* node,
                                printFunc printCB );


//------------------------------------------------------------------------
//
//  M a c r o   D e f i n i t i o n s
//
//  Define some helper functions that get defined as inlines so they will most
//  likely disappear when the code is compiled without debug enabled.
//

//
//  Given a btree control structure, return the number of records stored in
//  the btree.
//
inline static unsigned int getCount ( btree_t* btree )
{
    return btree->count;
};


//
//  If the specified node is at level 0, it's a leaf node.
//
inline static bool isLeaf ( bt_node_t* node )
{
    return ( node->level == 0 );
};


//
//  Define the local functions that convert node offsets to addresses and vice
//  versa.  Some of these are defined as static functions to give the compiler
//  the option of inlining them if it wants.
//
inline static void* cvtToAddr ( btree_t* btree,
                                offset_t offset )
{
    if ( btree->type == TYPE_USER )
    {
        return (void*)( offset + (void*)smControl );
    }
    else
    {
        return (void*)( offset + (void*)sysControl );
    }
}


inline static offset_t cvtToOffset ( btree_t* btree,
                                     void*    address )
{
    if ( btree->type == TYPE_USER )
    {
        return (offset_t)( address - (void*)smControl );
    }
    else
    {
        return (offset_t)( address - (void*)sysControl );
    }
}


//
//  Given a pointer to a node and the index of a record, return a pointer to
//  the child node at the specified index.
//
static inline bt_node_t* getChildPtr ( btree_t*   btree,
                                       bt_node_t* node,
                                       int        index )
{
    if ( isLeaf ( node ) )
    {
#ifdef BTREE_VERBOSE
        BLOG ( "Warning: Attempt to getChildPtr of leaf node: %p:%u\n",
               node, index );
#endif
        return NULL;
    }
    void* addr = cvtToAddr ( btree,
                             ((offset_t*)cvtToAddr(btree, node->children))[index] );
#ifdef BTREE_VERBOSE
    BLOG ( "    getChildPtr of %p:%u returns %p\n", node, index, addr );
#endif

    return addr;
}


//
//  Given a pointer to a node and the index of a record, return a pointer to
//  the left child node of that record.  This is the same thing as getting the
//  left child of an index.
//
static inline bt_node_t* getLeftChild ( btree_t*   btree,
                                        bt_node_t* node,
                                        int        index )
{
    if ( isLeaf ( node ) )
    {
#ifdef BTREE_VERBOSE
        BLOG ( "Warning: Attempt to getLeftChild of leaf node: %p:%u\n",
              node, index );
#endif
        return NULL;
    }
    void* addr = getChildPtr ( btree, node, index );

#ifdef BTREE_VERBOSE
    BLOG ( "    getLeftChild of %p:%u returns %p\n", node, index, addr );
#endif

    return addr;
}


//
//  Given a pointer to a node and the index of a record, return a pointer to
//  the right child node of that record.
//
static inline bt_node_t* getRightChild ( btree_t*   btree,
                                         bt_node_t* node,
                                         int        index )
{
    if ( isLeaf ( node ) )
    {
#ifdef BTREE_VERBOSE
        BLOG ( "Warning: Attempt to getRightChild of leaf node: %p:%u\n",
               node, index );
#endif
        return NULL;
    }
    void* addr = getChildPtr ( btree, node, index+1 );

#ifdef BTREE_VERBOSE
   BLOG ( "    getRightChild of %p:%u returns %p\n", node, index, addr );
#endif

    return addr;
}


static inline offset_t getChild ( btree_t*   btree,
                                  bt_node_t* node,
                                  int        index )
{
    if ( isLeaf ( node ) )
    {
#ifdef BTREE_VERBOSE
        BLOG ( "Warning: Attempt to getChild of leaf node: %p:%u\n", node, index );
#endif
        return 0;
    }
    offset_t offset = ( (offset_t*)cvtToAddr(btree, node->children) )[index];

#ifdef BTREE_VERBOSE
    BLOG ( "    getChild of %p:%u returns %lu\n", node, index, offset );
#endif

    return offset;
}


static inline void setChild ( btree_t*   btree,
                              bt_node_t* node,
                              int        index,
                              offset_t   value )
{
    if ( isLeaf ( node ) )
    {
#ifdef BTREE_VERBOSE
        BLOG ( "Warning: Attempt to setChild of leaf node: %p:%u\n", node, index );
#endif
        return;
    }

    ( (offset_t*)cvtToAddr(btree, node->children) )[index] = value;

    return;
}


//
//  Given a pointer to a node and the index of a record, return a pointer to
//  the user data structure stored as the data item of that record.
//
static inline void* getDataAddress ( btree_t*   btree,
                                     bt_node_t* node,
                                     int        index )
{
    return cvtToAddr ( btree, node->dataRecords + index * sO );
}


//
//  Given a pointer to a node and the index of a record, return the address of
//  the user's data structure stored at that position in the node.
//
static offset_t* getDataRecord ( btree_t*   btree,
                                 bt_node_t* node,
                                 int        index )
{
#ifdef BTREE_VERBOSE
    // BLOG ( "====> In getDataRecord macro...\n" );
    // BLOG ( "====> node: %p, index: %u\n", node, index );
    // BLOG ( "====>   address of: %p\n", cvtToAddr(btree,node->dataRecords));
    // BLOG ( "====>   indexing..: %lx\n",
    //       ((offset_t*)cvtToAddr(btree,node->dataRecords))[index] );
    // BLOG ( "====>   address of: %p\n",
    //       toAddress(((offset_t*)cvtToAddr(btree,node->dataRecords))[index]) );
#endif
    return toAddress(((offset_t*)cvtToAddr(btree,node->dataRecords))[index]);
}


static inline void setRecord ( btree_t*   btree,
                               bt_node_t* node,
                               int        index,
                               offset_t   value )
{
    *(offset_t*)getDataAddress ( btree, node, index ) = value;
}


//
//  Move a block of data records given the source node and index, the
//  destination node and index, and the number of items to be moved.  Note
//  that the size is NOT the number of bytes to move!
//
static inline void moveRecords ( btree_t*     btree,
                                 bt_node_t*   srcNode,
                                 int          srcIndex,
                                 bt_node_t*   dstNode,
                                 int          dstIndex,
                                 unsigned int count )
{
    BLOG ( "    moveRecords moving %u records from %p:%u to %p:%u\n",
           count, srcNode, srcIndex, dstNode, dstIndex );

    if ( count != 0 )
    {
        memmove ( cvtToAddr ( btree, dstNode->dataRecords + ( dstIndex * sO ) ),
                  cvtToAddr ( btree, srcNode->dataRecords + ( srcIndex * sO ) ),
                  count * sO );
    }
}


//
//  Move a block of child node pointers given the source node and index, the
//  destination node and index, and the number of items to be moved.  Note
//  that the size is NOT the number of bytes to move!
//
static inline void moveChildren ( btree_t*     btree,
                                  bt_node_t*   srcNode,
                                  int          srcIndex,
                                  bt_node_t*   dstNode,
                                  int          dstIndex,
                                  unsigned int count )
{
    if ( count > 0 )
    {
        if ( isLeaf ( srcNode ) && ! isLeaf ( dstNode ) )
        {
#ifdef BTREE_VERBOSE
            BLOG ( "Info: Attempt to moveChildren of leaf node - Ignored: "
                   "%p:%u or %p:%u\n", srcNode, srcIndex, dstNode, dstIndex );
#endif
        }
        else
        {
            BLOG ( "    moveChildren moving %u records from %p:%u to %p:%u\n",
                  count, srcNode, srcIndex, dstNode, dstIndex );

            memmove ( cvtToAddr ( btree, dstNode->children + ( dstIndex * sO ) ),
                      cvtToAddr ( btree, srcNode->children + ( srcIndex * sO ) ),
                      count * sO );
        }
    }
    return;
}


static inline void copyRecord ( btree_t*     btree,
                                bt_node_t*   srcNode,
                                int          srcIndex,
                                bt_node_t*   dstNode,
                                int          dstIndex )
{
    BLOG ( "    copyRecord copying %p:%u to %p:%u\n",
           srcNode, srcIndex, dstNode, dstIndex );

    *(offset_t*)getDataAddress ( btree, dstNode, dstIndex ) =
        *(offset_t*)getDataAddress ( btree, srcNode, srcIndex );
}


static inline void copyChild ( btree_t*     btree,
                               bt_node_t*   srcNode,
                               int          srcIndex,
                               bt_node_t*   dstNode,
                               int          dstIndex )
{
    if ( isLeaf ( srcNode ) && ! isLeaf ( dstNode ) )
    {
#ifdef BTREE_VERBOSE
        BLOG ( "Info: Attempt to copyChild of leaf node - Ignored: %p:%u or %p:%u\n",
              srcNode, srcIndex, dstNode, dstIndex );
#endif
    }
    else
    {
        BLOG ( "    copyChild copying %p:%u[%p] to %p:%u[%p]\n",
               srcNode, srcIndex,
               cvtToAddr ( btree, getChild ( btree, srcNode, srcIndex ) ),
               dstNode, dstIndex,
               cvtToAddr ( btree, getChild ( btree, dstNode, dstIndex ) ) );

        setChild ( btree, dstNode, dstIndex, getChild ( btree, srcNode,
                   srcIndex ) );
    }
}


//
//  Define the cleanup handler for the B-tree mutex.  This handler will be
//  executed if this thread is cancelled while it is waiting.  What this
//  function needs to do is release the mutex that this thread is holding when
//  it enters the wait state.  Without this cleanup handler, the mutex will
//  remain locked when this thread is cancelled and almost certainly cause a
//  hang condition in the code the next time someone tries to lock this same
//  mutex.
//
static void mutexCleanupHandler ( void* arg )
{
    BLOG ( "Warning: The mutexCleanupHandler has been executed!\n" );
    pthread_mutex_unlock ( arg );
}


//
//  Lock and protect the shared memory manager mutex.
//
//  NOTE: It is REQUIRED that the "push" and "pop" cleanup functions be
//  executed in the same lexical nesting level in the same function so the
//  "lock" and "unlock" macros must be used at the same lexical nesting level
//  and in the same function as well.  These functions also had to be
//  implemented as macros instead of functions because of the lexical level
//  limitation.
//
#ifdef BTREE_LOCKS_ENABLE
#define BTREE_LOCK                                                              \
    int btreeLockStatus = 0;                                                    \
    if ( ( btreeLockStatus = pthread_mutex_lock ( &smControl->smLock ) ) != 0 ) \
    {                                                                           \
        printf ( "Error: Unable to acquire the shared memory memory manager "   \
                 "lock: %d[%s]\n", btreeLockStatus, strerror(btreeLockStatus) ); \
    }                                                                           \
    pthread_cleanup_push ( mutexCleanupHandler, &smControl->smLock );

//
//  Unlock the shared memory manager mutex.
//
#define BTREE_UNLOCK                             \
    pthread_cleanup_pop ( 0 );                   \
    pthread_mutex_unlock ( &smControl->smLock );
#else
#   define BTREE_LOCK
#   define BTREE_UNLOCK
#endif


/*!----------------------------------------------------------------------------

    b t r e e _ i n i t i a l i z e

    @brief Initialize all of the btree parameters.

    This function will initialize all of the parameters that we will use to
    build any of the btrees the user wants to define.  This only needs to be
    done once since the parameters are the same of every node of each btree.

    @param[in] - handle - The base address of the shared memory segment.

    @return  0 - Everything was created without error.
            !0 - There was an error in the process.

-----------------------------------------------------------------------------*/
//
//  NOTE: The btree->type field MUST be set before calling this function!
//
void btree_initialize ( btree_t*       btree,
                        unsigned int   maxRecordsPerNode,
                        btree_key_def* keyDefinition )
{
    BLOG ( "In btree_initialize\n" );

    //
    //  Compute and initialize all of the B-tree parameters for the given
    //  number of records per node requested by the caller.  If the user
    //  specifies an even number of records, increment it to be an odd number.
    //  The btree algorithm here only operates correctly if the record count
    //  is odd.
    //
    if ( ( maxRecordsPerNode & 1 ) == 0 )
    {
        maxRecordsPerNode++;
    }
    btree->maxRecCnt = maxRecordsPerNode;

    //
    //  Compute the node size as the btree node header size plus the size of
    //  the record offset area plus the link offfset area.
    //
    btree->nodeSize = sN + ( maxRecordsPerNode * sO ) +
                      ( ( maxRecordsPerNode + 1 ) * sO );
    //
    //  Compute the value of the "minimum degree" (i.e. "t") value.
    //
    btree->minDegree = ( maxRecordsPerNode + 1 ) / 2;

    //
    //  Compute the minimum number of records in each node.
    //
    btree->min = btree->minDegree - 1;

    //
    //  Initialize the current record count to zero.
    //
    btree->count = 0;

    //
    //  Store the key definition data.
    //
    btree->keyDef = cvtToOffset ( btree, keyDefinition );

    //
    //  Allocate the root node for this b-tree and set it into the btree
    //  structure.
    //
    btree->root = cvtToOffset ( btree, allocate_btree_node ( btree ) );

    //
    //  If we are in debug mode, display the values of all of the B-tree
    //  parameters we just initialized.
    //
    btree_print_header ( btree );

    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ c r e a t e

    @brief Create a new btree instance.

    This function is used to create a btree with just an empty root node.
    Once this function is called, the btree is ready to use.

    All of the arguments are callback functions that will be used to
    manipulate the btree and allocate/free nodes of the tree.

    If the memory allocation functions are not specified (by passing in a NULL
    pointer), the normal shared memory page "page_malloc" and "page_free"
    functions will be used.  Note that if the user specifies the allocate/free
    functions, the btree thus created cannot be used in a shared memory
    segment to exchange data with other processes or threads.

    @param[in] maxRecordsPerNode - The maximum number of records per node
    @param[in] numberOfKeys - The number of fields in the key
    @param[in] keyNindex - The index into the record of each key field

    @return The pointer to an empty B-tree.
            NULL if an error occurs.

-----------------------------------------------------------------------------*/
btree_t* btree_create ( unsigned int   maxRecordsPerNode,
                        btree_key_def* keyDefinition )
{
    btree_t* btree;

    BLOG ( "In btree_create\n" );

    //
    //  Go allocate a shared memory chunk for this new btree structure.
    //
    btree = sm_malloc ( sizeof(btree_t) );
    if ( btree == NULL )
    {
        errno = ENOMEM;
        printf ( "Error: Unable to allocate memory for btree: %d[%s]\n", errno,
                 strerror(errno) );
#ifdef BTREE_DEBUG
        exit ( 255 );
#else
        return NULL;
#endif
    }
    btree_create_in_place ( btree, maxRecordsPerNode, keyDefinition );

    //
    //  Return the pointer to the btree to the caller.
    //
    return btree;
}


/*!----------------------------------------------------------------------------

    b t r e e _ c r e a t e _ i n _ p l a c e

    @brief Create a new B-tree instance.

    This function will create a new B-tree instance for those cases in which
    the caller wants to supply the btree_t data structure.  This function is
    identical to the "btree_create" function above except for who allocates
    the storage for the B-tree data.

    This function also differs from the above function in that it does not
    return any value (since the btree_t object already exists as an input
    argument.

    @param[in] btree - The address of the btree_t data object to be populated
    @param[in] maxRecordsPerNode - The maximum number of records per node
    @param[in] numberOfKeys - The number of fields in the key
    @param[in] keyNindex - The index into the record of each key field

    @return None

-----------------------------------------------------------------------------*/
void btree_create_in_place ( btree_t*       btree,
                             unsigned int   maxRecordsPerNode,
                             btree_key_def* keyDefinition )
{
    BLOG ( "In btree_create_in_place\n" );

    //
    //  Set the type of B-tree this is (user vs. system).
    //
    btree->type = TYPE_USER;

    //
    //  Go initialize the supplied btree structure.
    //
    btree_initialize ( btree, maxRecordsPerNode, keyDefinition );

    //
    //  Return to the caller.
    //
    return;
}



/*!----------------------------------------------------------------------------

    b t r e e _ c r e a t e _ s y s

    @brief

    This function will

    TODO: This function is probably obsolete.

    @param[in]
    @param[out]
    @param[in,out]

    @return None

-----------------------------------------------------------------------------*/
btree_t* btree_create_sys ( btree_t*       btree,
                            unsigned int   maxRecordsPerNode,
                            btree_key_def* keyDefinition )
{
    BLOG ( "In btree_create_sys\n" );

    //
    //  Set the type of B-tree this is (user vs. system).
    //
    btree->type = TYPE_SYSTEM;

    //
    //  Go initialze the new btree data structure as a system btree.
    //
    btree_initialize ( btree, maxRecordsPerNode, keyDefinition );

    //
    //  Return the pointer to the btree to the caller.
    //
    return btree;
}


/*!----------------------------------------------------------------------------

    i n i t _ n o d e _ h e a d e r

    @brief Initialize a B-tree node header.

    This function will

    @param[in]
    @param[out]
    @param[in,out]

    @return None

-----------------------------------------------------------------------------*/
static void init_node_header ( btree_t* btree,
                               bt_node_t* node )
{
    BLOG ( "In init_node_header\n" );

    //
    //  Initialize the whole node structure to zeroes.
    //
#ifdef BTREE_DEBUG
    memset ( node, 'N', btree->nodeSize );
#else
    memset ( node, 0, btree->nodeSize );
#endif

    //
    //  Set the number of records in this node to zero.
    //
    node->keysInUse = 0;

    //
    //  Initialize the offset of the beginning of the data records in this
    //  node structure.
    //
    node->dataRecords = cvtToOffset ( btree, (char*)node + sN );

    //
    //  Initialize the offset of the beginning of the child pointers in this
    //  node structure.
    //
    node->children = node->dataRecords + sD;

    //
    //  Set the tree level to zero.
    //
    node->level = 0;

    //
    //  Set the linked list pointer to NULL.
    //
    node->next = 0;

    //
    //  Set the parent node link to NULL.
    //
    node->parent = 0;

    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    a l l o c a t e _ b t r e e _ n o d e

    @brief Allocate a new btree node.

    This function will allocate a new btree node structure and initialize it.

    @param[in] btree - The address of the btree being extended.

    @return The address of the newly allocated btree node structure.
            NULL if there was an error allocating the node.

-----------------------------------------------------------------------------*/
static bt_node_t* allocate_btree_node ( btree_t* btree )
{
    bt_node_t* node;

    BLOG ( "In allocate_btree_node\n" );

    //
    //  Create a new node data structure.
    //
    if ( btree->type == TYPE_USER )
    {
        node = sm_malloc ( btree->nodeSize );
    }
    else
    {
        node = sm_malloc_sys ( btree->nodeSize );
    }
    //
    //  If the allocation failed, generate an error message and quit.
    //
    if ( node == NULL )
    {
        printf ( "Error: Unable to allocate a new btree node structure: %d[%s]\n",
                 errno, strerror(errno) );
        return NULL;
    }

    BLOG ( "  allocate_btree_node - Allocated %p\n", node );

    //
    //  Go initialize the header of the new node.
    //
    init_node_header ( btree, node );

    //
    //  Return the address of the new node to the caller.
    //
    return node;
}


/*!----------------------------------------------------------------------------

    f r e e _ b t r e e _ n o d e

    @brief Deallocate an existing btree node strructure.

    This function will deallocate the memory used by an existing btree node
    data structure.

    @param[in] btree - The address of the btree control structure.
    @param[in] node - The address of the btree node to be deallocated.

    @return None

-----------------------------------------------------------------------------*/
static void free_btree_node ( btree_t*   btree,
                              bt_node_t* node )
{
    BLOG ( "In free_btree_node - Freeing %p\n", node );

    //
    //  Go erase everything in this chunk of memory (for safety/security
    //  sake).
    //
#ifdef BTREE_DEBUG
    memset ( node, 0x55, btree->nodeSize );
#else
    memset ( node, 0, btree->nodeSize );
#endif

    //
    //  Go return this block of memory to the memory pool.
    //
    if ( btree->type == TYPE_USER )
    {
        sm_free ( node );
    }
    else
    {
        sm_free_sys ( node );
    }
    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ s p l i t _ c h i l d

    @brief Split a child node.

    This function will split the specified child node into 2 nodes and move
    the data at the "index" position in the original node up to the parent
    node (at the appropriate place).

    This function will create a new node to the right of the given child node.
    The current child node will be divided into 3 pieces.  The part from index
    0 up to index minDegree (the midpoint in the node) will be left in the
    child undisturbed.  The record at the midpoint will be moved up to the
    parent node at the index specified.  The rest of the data in the child
    node from past the midpoint to the end of the node will be moved into the
    new child node starting at index 0.

    @param[in] btree - The btree to operate on
    @param[in] parent - The address of the parent node
    @param[in] index - The index position in the parent to insert the new record
    @param[in] child - The address of the child node to be split

    @return None

-----------------------------------------------------------------------------*/
static void btree_split_child ( btree_t*     btree,
                                bt_node_t*   parent,
                                unsigned int index,
                                bt_node_t*   child )
{
    int          i;
    unsigned int min       = btree->min;
    unsigned int minDegree = btree->minDegree;
    bt_node_t*   newChild  = 0;

    BLOG ( "In btree_split_child\n" );

    //
    //  Go create a new tree node as our "new child".  The new node will be
    //  created at the same level as the old node (at least initially - It may
    //  get moved when we balance the tree later) and to the right of the node
    //  that is being split.
    //
    newChild = allocate_btree_node ( btree );

    //
    //  Initialize the fields of the new node appropriately.
    //
    newChild->level     = child->level;
    newChild->keysInUse = btree->min;
    newChild->parent    = cvtToOffset ( btree, parent );

    //
    //  Move the keys from beyond the split point from the old child to the
    //  beginning of the new child.  This loop will copy all the records in
    //  the full node past the minDegree mark (the center of the node) into
    //  the beginning of the new child that we just created.  This is best
    //  illustrated with an example... Suppose our node has a max size of 21
    //  records.  Min is then 10, the minDegree is 11.  So "minDegree" is the
    //  point at which we will split the node.  Everything less than minDegree
    //  will be left in the existing child.  Everything greater than the
    //  minDegree will be copied into the beginning of the new child we just
    //  allocated.  The record at the split point (minDegree) will be moved up
    //  to the parent node at the index position specified by the caller.
    //
    //  Now move the upper part of the old node into the new child.  In our
    //  example, this will move indices 11-20 into the new child at 0-9.
    //
    for ( i = 0; i < min; ++i )
    {
        copyRecord ( btree, child, minDegree + i, newChild, i );

        //
        //  If this is not a leaf node, also copy the "children" pointers from
        //  the old child to the new one.
        //
        if ( ! isLeaf ( child ) )
        {
            copyChild ( btree, child, minDegree + i, newChild, i );

            //
            //  Make the parent pointer in the child that we just moved now
            //  point to the new child rather than the old child from which it
            //  was moved.
            //
            getLeftChild ( btree, newChild, i )->parent =
                cvtToOffset ( btree, newChild );
        }
    }
    //
    //  If this is not a leaf node, copy the last node pointer (remember that
    //  there is one extra node pointer for a given number of keys).
    //
    if ( ! isLeaf ( child ) )
    {
        copyChild ( btree, child, minDegree + i, newChild, i );

        //
        //  Make the parent pointer in the child that we just moved now point
        //  to the new child rather than the old child from which it was
        //  moved.
        //
        getLeftChild ( btree, newChild, i )->parent =
            cvtToOffset ( btree, newChild );
    }
    //
    //  Adjust the record count of the old child to be correct now that we've
    //  copied out about half of the data in this node.
    //
    child->keysInUse = btree->min;

    //
    //  If this insertion is not at the end of the data records in the
    //  parent...
    //
    if ( index < parent->keysInUse )
    {
        //
        //  Move all the child pointers of the parent up (towards the end) of
        //  the parent node to make room for the record we are going to move
        //  up to the parent from the old child.
        //
        moveChildren ( btree, parent, index, parent, index + 1,
                       parent->keysInUse - index + 1 );
        //
        //  Move all the data records in the parent one place to the right as
        //  well to make room for the new child record.
        //
        moveRecords ( btree, parent, index, parent, index + 1,
                      parent->keysInUse - index + 1 );
    }
    //
    //  Store the new child we just created into the node list of the parent
    //  as the right child of the index location.
    //
    setChild ( btree, parent, index + 1, cvtToOffset ( btree, newChild ) );

    //
    //  Move the key that was used to split the node from the child to the
    //  parent.  Note that it was split at the "index" value specified by the
    //  caller.
    //
    copyRecord ( btree, child, btree->min, parent, index );

    //
    //  Increment the number of records now in the parent.
    //
    parent->keysInUse++;

    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i n s e r t _ n o n f u l l

    @brief Insert a record into a tree with a non-full root node.

    This function will insert a record into a tree with a non-full root node.

    TODO: We could make the node search more efficient by implementing a
    binary search instead of the linear one.  For small tree orders, the
    linear search is more efficient but as the tree orders get larger, the
    binary search will win.

    @param[in] btree - The btree to operate on
    @param[in] parent - The address of the parent node
    @param[in] data - The address of the data to be inserted

    @return None

-----------------------------------------------------------------------------*/
static void btree_insert_nonfull ( btree_t*   btree,
                                   bt_node_t* parentNode,
                                   void*      data )
{
    int        i;
    bt_node_t* child;
    bt_node_t* node = parentNode;

    BLOG ( "In btree_insert_nonfull\n" );

//
//  Note that his "goto" has been implemented to eliminate a recursive call to
//  this function.
//
insert:

    //
    //  Position us at the end of the records in this node.
    //
    i = node->keysInUse - 1;

    //
    //  Initialize the source and destination pointers for the following
    //  loops...
    //
    offset_t* src = getDataAddress ( btree, node, i );
    offset_t* dst = src + 1;

    BLOG ( "  Setup - i: %d, src: 0x%p, dst: 0x%p\n", i, src, dst );

    //
    //  If this is a leaf node...
    //
    if ( isLeaf ( node )  )
    {
        //
        //  Move all the records in the node one position to the right
        //  (working backwards from the end) until we get to the point where
        //  the new data will go.
        //
        //  TODO: Make this a block move operation.
        //
        while ( i >= 0 &&
                ( btree_compare_function ( btree, data,
                                           toAddress ( *src ) ) < 0 ) )
        {
            BLOG ( "  Copying - i: %d, src: 0x%p, dst: 0x%p\n", i, src, dst );

            *dst-- = *src--;
            i--;
        }
        //
        //  Put the new data into this node and increment the number of
        //  records in the node.
        //
        BLOG ( "  Inserting - dst: 0x%p\n", dst );
        *dst = toOffset ( data );
        node->keysInUse++;
    }
    //
    //  If this is not a leaf node...
    //
    else
    {
        //
        //  Search the node records backwards from the end until we find the
        //  point at which the new data should go.
        //
        while ( i >= 0 &&
                ( btree_compare_function ( btree, data,
                                           cvtToAddr (btree, *src) ) < 0 ) )
        {
            i--;
        }
        //
        //  Grab the right hand child of this position so we can check to see
        //  if the new data goes into the current node or the right child
        //  node.
        //
        child = getRightChild ( btree, node, i );
        i++;

        //
        //  If the child node is full...
        //
        if ( child->keysInUse >= btree->maxRecCnt )
        {
            //
            //  Go split this node into 2 nodes.
            //
            btree_split_child ( btree, node, i, child );

            //
            //  If the new data is greater than the current record then
            //  increment to the next record.
            //
            if ( btree_compare_function ( btree, data,
                                          getDataRecord ( btree, node, i ) ) > 0 )
            {
                i++;
            }
        }
        //
        //  Reset the starting node and go back up and try to insert this
        //  again.  Note that this "goto" actually implements the same thing
        //  as a recursive call except it's more efficient and doesn't blow
        //  out the stack.
        //
        node = getLeftChild ( btree, node, i );
        goto insert;
    }
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i n s e r t

    @brief Insert a new data record into the btree.

    This function will insert a new data record into the specified btree.

    @param[in] btree - The btree to operate on
    @param[in] data - The address of the data to be inserted

    @return Always 0!  TODO: Fix this?

-----------------------------------------------------------------------------*/
int btree_insert ( btree_t* btree, void* data )
{
    bt_node_t* rootNode;

    BLOG ( "In btree_insert\n" );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Start the search at the root node.
    //
    rootNode = cvtToAddr ( btree, btree->root );

    //
    //  If the root node is full then we'll need to create a new node that
    //  will become the root and the full root will be split into 2 nodes that
    //  are each approximately half full.
    //
    //  Note that we will need 2 new nodes to complete the process of
    //  splitting the root node.
    //
    if ( rootNode->keysInUse >= btree->maxRecCnt )
    {
        bt_node_t* newRoot;

        BLOG ( "We need to split the root node which is full...\n" );

        //
        //  Go get the new root node.
        //
        newRoot = allocate_btree_node ( btree );

        //
        //  Increment the level of this new root node to be one more than the
        //  old btree root node.  We are making the whole tree one level
        //  deeper.
        //
        newRoot->level = rootNode->level + 1;

        //
        //  Make the new root node now point to the old root node.
        //
        newRoot->next = cvtToOffset ( btree, rootNode );

        //
        //  Make the new node we just created the root node.
        //
        btree->root = cvtToOffset ( btree, newRoot );

        //
        //  Initialize the rest of the fields in the new root node and make
        //  the old root the first child of the new root node (the left
        //  child).
        //
        newRoot->keysInUse = 0;

        setChild ( btree, newRoot, 0, cvtToOffset ( btree, rootNode ) );

        rootNode->parent = cvtToOffset ( btree, newRoot );

        //
        //  Go split the old root node into 2 nodes and slosh the data between
        //  them.
        //
        btree_split_child ( btree, newRoot, 0, rootNode );

        BLOG ( "  Finished splitting the root node...\n" );

        //
        //  Go insert the new data we are trying to insert into the new tree
        //  that has the new node as it's root now.
        //
        btree_insert_nonfull ( btree, newRoot, data );
    }
    //
    //  If the root node is not full then just go insert the new data into
    //  this tree somewhere.
    //
    else
    {
        btree_insert_nonfull ( btree, rootNode, data );
    }
    //
    //  Increment the number of records in the btree.
    //
    ++btree->count;

    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    //
    //  Return the completion code to the caller.
    //
    return 0;
}


/*!----------------------------------------------------------------------------

    g e t _ m a x _ k e y _ p o s

    @brief Used to get the position of the maximum key within the subtree.

    This function will find the position of the maximum key within the
    subtree.

    @param[in] btree - The btree to operate on
    @param[in] subtree - The address of the root node of the subtree to search.

    @return nodePosition - The position of the maximum data value in the subtree.
                           If the returned node value is zero, this tree is
                           empty.

-----------------------------------------------------------------------------*/
static nodePosition get_max_key_pos ( btree_t*   btree,
                                      bt_node_t* subtree )
{
    nodePosition nodePosition = { 0, 0 };
    bt_node_t*   node         = subtree;

    BLOG ( "In get_max_key_pos\n" );

    if ( btree->count > 0 )
    {
        //
        //  Initialize the node to the specified subtree starting point.
        //
        nodePosition.node = subtree;

        //
        //  This loop basically runs down the right hand node pointers in each node
        //  until it gets to the leaf node at which point the value we want is the
        //  last one (right most) in that node.
        //
        while ( ! isLeaf ( node ) )
        {
            //
            //  Make the current node the right most child node of the current node.
            //
            node = getRightChild ( btree, node, node->keysInUse - 1 );
        }
        //
        //  Save the current node pointer and the index of the last record in
        //  this node.
        //
        nodePosition.node  = node;
        nodePosition.index = node->keysInUse - 1;
    }
    //
    //  Return the node position data to the caller.
    //
    return nodePosition;
}


/*!----------------------------------------------------------------------------

    g e t _ m i n _ k e y _ p o s

    @brief Used to get the position of the minimum key within the subtree.

    This function will find the position of the minimum key within the
    subtree.

    @param[in] btree - The btree to operate on
    @param[in] subtree - The address of the root node of the subtree to search.

    @return nodePosition - The position of the minimum data value in the subtree.
                           If the returned node value is zero, the btree is
                           empty.

-----------------------------------------------------------------------------*/
static nodePosition get_min_key_pos ( btree_t*   btree,
                                      bt_node_t* subtree )
{
    nodePosition nodePosition = { 0, 0 };
    bt_node_t*   node         = subtree;

    BLOG ( "In get_min_key_pos\n" );

    //
    //  If this btree is not an empty tree...
    //
    if ( btree->count > 0 )
    {
        //
        //  Initialize the node to the specified subtree starting point.
        //
        nodePosition.node = subtree;

        //
        //  This loop basically runs down the left hand node pointers in each node
        //  until it gets to the leaf node at which point the value we want is the
        //  first one (left most) in that node.
        //
        while ( ! isLeaf ( node ) )
        {
            //
            //  Make the current node the left child node of the current node.
            //
            node = getLeftChild ( btree, node, 0 );
        }
        //
        //  Save the current node pointer and the index of the first record in
        //  this node.
        //
        nodePosition.node  = node;
        nodePosition.index = 0;
    }
    //
    //  Return the node position data to the caller.
    //
    return nodePosition;
}


/*!----------------------------------------------------------------------------

    m e r g e _ s i b l i n g s

    @brief Merge 2 child nodes into one.

    This function will merge the 2 child nodes of the record defined by the
    parent node and index values into a single node.  The left child of the
    specified record will be kept and the record at the index point in the
    parent will be moved into the left child.  The remaining records in the
    right child will then be copied into the left child as well.

    This is case 3b from the Cormen book.

    This function makes some assumptions:

    The left and right children both have min records.
    The parent has at least one record.

    @param[in] btree - The btree to operate on
    @param[in] parent - The address of the parent node
    @param[in] index - The index in the parent node whose children are being
                       merged.

    @return The address of the new merged node.

-----------------------------------------------------------------------------*/
static bt_node_t* merge_siblings ( btree_t*     btree,
                                   bt_node_t*   parent,
                                   unsigned int index )
{
    bt_node_t* leftChild;
    bt_node_t* rightChild;
    offset_t   newParent;

    BLOG ( "In merge_siblings\n" );

    //
    //  If the index is the last position in use in the parent, back up one
    //  position.
    //
    if ( index == ( parent->keysInUse ) )
    {
        index--;
    }
    //
    //  Get the pointers to the left and right children of the record in the
    //  parent node at the specified index.
    //
    leftChild  = getLeftChild  ( btree, parent, index );
    rightChild = getRightChild ( btree, parent, index );

    //
    //  Move the data record from the parent at the index to the left child at
    //  the split point.
    //
    copyRecord ( btree, parent, index, leftChild, leftChild->keysInUse++ );

    //
    //  Move all of the data records from the right child into the left child.
    //
    moveRecords ( btree, rightChild, 0, leftChild, leftChild->keysInUse,
                  rightChild->keysInUse );
    //
    //  Increment the keysInUse by the number of records we just moved from
    //  the right child plus one extra for the parent node that we moved down.
    //
    leftChild->keysInUse += rightChild->keysInUse;

    //
    //  If this is not a leaf node, move the child pointers from the right
    //  child into the left child.
    //
    if ( ! isLeaf ( leftChild ) )
    {
        moveChildren ( btree, rightChild, 0, leftChild, leftChild->keysInUse,
                       rightChild->keysInUse + 1 );
        //
        //  Update the parent pointers on all of the nodes pointed to by the
        //  child records we just moved so they now point to the new leftChild
        //  node.
        //
        //  TODO: Make this a block move operation.
        //
        newParent = cvtToOffset ( btree, leftChild );
        for ( int i = 0; i <= rightChild->keysInUse; ++i )
        {
            getChildPtr ( btree, rightChild, i )->parent = newParent;
        }
    }
    //
    //  If the parent node is not empty, now that we moved the specified data
    //  record from it to the new merged node...
    //
    if ( ( parent->keysInUse ) > 1 )
    {
        //
        //  Decrement the number of records in the parent.
        //
        parent->keysInUse--;

        //
        //  Move all of the data records and child pointers one spot to the
        //  left in the parent node to close up the gap left when we moved the
        //  data record out to the leftChild.
        //
        moveRecords ( btree, parent, index+1, parent, index, parent->keysInUse );

        moveChildren ( btree, parent, index+2, parent, index+1, parent->keysInUse );
    }
    //
    //  If the parent node is now empty, it must be the root node so make the
    //  merged node we just created (the leftChild) the new root.
    //
    else
    {
        free_btree_node ( btree, parent );

        leftChild->parent = 0;

        btree->root = cvtToOffset ( btree, leftChild );
    }
    //
    //  Update the node chain pointers to be correct now that we will be
    //  deleting the right child node.
    //
    leftChild->next = rightChild->next;

    //
    //  Go free up the empty right child node.
    //
    free_btree_node ( btree, rightChild );

    //
    //  Return the merged left child node to the caller.
    //
    return leftChild;
}


/*!----------------------------------------------------------------------------

    m o v e _ k e y

    @brief Move a record between the left and right siblings of the specified
    node.

    This function will move a record between the left and right siblings of
    the specified node.

    @param[in] btree - The address of the btree to operate on
    @param[in] node - The address of the node to move the key out of
    @param[in] index - The position of the key within the node to be moved
    @param[in] pos - The left/right indicator for the destination child

    @return None

-----------------------------------------------------------------------------*/
static void move_key ( btree_t* btree, bt_node_t* node, unsigned int index,
                       position_t pos )
{
    BLOG ( "In move_key: btree[%p], node[%p], index[%u], direction: %s\n",
            btree, node, index, pos==left ? "Left" : "Right" );
    //
    //  If we are going to go to the right, back up one position.
    //
    if ( pos == right )
    {
        index--;
    }
    //
    //  Get the left and right child pointers.
    //
    bt_node_t* lchild = getLeftChild  ( btree, node, index );
    bt_node_t* rchild = getRightChild ( btree, node, index );

    //
    //  If we are moving the record into the left child...
    //
    if ( pos == left )
    {
        //
        //  Move the data record from the parent at the specified index to the
        //  left child as the highest data record in that child.
        //
        copyRecord ( btree, node, index, lchild, lchild->keysInUse );

        //
        //  Copy the right subtree pointer to the new left child entry we just
        //  created.
        //
        copyChild ( btree, rchild, 0, lchild, lchild->keysInUse + 1 );

        //
        //  Increment the number of keys in use in the left child node.
        //
        lchild->keysInUse++;

        //
        //  Copy the minimum record from the right child node to the parent
        //  node at the specified index, replacing the original data value.
        //
        copyRecord ( btree, rchild, 0, node, index );

        //
        //  Move the data records and children pointers all one position to
        //  the left in the right child because we have moved the first
        //  (leftmost) data value out of this node.
        //
        moveRecords  ( btree, rchild, 1, rchild, 0, rchild->keysInUse - 1 );
        moveChildren ( btree, rchild, 1, rchild, 0, rchild->keysInUse );

        //
        //  Decrement the number of data records in the right child node.
        //
        rchild->keysInUse--;
    }
    //
    //  If we are moving the record into the right child...
    //
    else
    {
        //
        //  Move the data records and child pointers all one position to the
        //  right (high end) of the node to make room for a new data record at
        //  index 0.
        //
        moveRecords  ( btree, rchild, 0, rchild, 1, rchild->keysInUse - 1 );
        moveChildren ( btree, rchild, 0, rchild, 1, rchild->keysInUse );

        //
        //  Copy the data record in the parent node at the specified index
        //  down into the first position of the right child.
        //
        copyRecord ( btree, node, index, rchild, 0 );

        //
        //  Copy the highest child data record from the left child into the
        //  lowest record of the right child.
        //
        copyChild ( btree, lchild, lchild->keysInUse, rchild, 0 );

        //
        //  Copy the left subtree pointers from the left child to the
        //  parent node at the specified index.
        //
        copyRecord ( btree, lchild, lchild->keysInUse - 1, node, index );

        //
        //  Adjust the number of keys in both the left and right children.
        //
        lchild->keysInUse--;
        rchild->keysInUse++;
    }
}


/*!----------------------------------------------------------------------------

    d e l e t e _ k e y _ f r o m _ n o d e

    @brief Delete the key at the specified position in the specified node.

    This function will delete the key at the specified position in the
    specified node.

    @param[in] btree - The btree to operate on
    @param[in] nodePosition - The node and index to operate on

    @return Status code

-----------------------------------------------------------------------------*/
static int delete_key_from_node ( btree_t* btree, nodePosition* nodePosition )
{
    bt_node_t*   node  = nodePosition->node;
    unsigned int index = nodePosition->index;

    BLOG ( "In delete_key_from_node\n" );

    //
    //  If this is not a leaf node, return an error code.  This function
    //  should never be called with anything other than a leaf node.
    //
    if ( ! isLeaf ( node )  )
    {
        printf ( "Error: delete_key_from_node called with a non-leaf node: "
                 "%p[%u]\n", node, index );
        return -1;
    }
    //
    //  Move all of the data records down from the position just past the data
    //  we want to delete to the end of the data to collapse the data list
    //  after the removal of the target record at location "index".
    //
    if ( node->keysInUse > 1 )
    {
        moveRecords ( btree, node, index + 1, node, index,
                      node->keysInUse - 1 - index );
    }
    //
    //  Decrement the number of keys in use in this node.
    //
    node->keysInUse--;

    //
    //  If the node is now empty and it is not the root node, go free it up.
    //
    //  Note that the root node is always present, even if the tree is empty.
    //
    if ( ( node->keysInUse == 0 ) && ( node != cvtToAddr ( btree, btree->root ) ) )
    {
        free_btree_node ( btree, node );
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}

/**
*       Function used to delete a node from a  B-Tree
*       @param btree The B-Tree
*       @param key Key of the node to be deleted
*       @param value function to map the key to an unique integer value
*       @param compare Function used to compare the two nodes of the tree
*       @return success or failure
*/
static int btree_delete_subtree ( btree_t* btree, bt_node_t* subtree, void* data )
{
    int          i;
    int          diff;
    unsigned int index;
    unsigned int splitPoint = btree->min;
    bt_node_t*   node = NULL;
    bt_node_t*   rsibling;
    bt_node_t*   lsibling;
    bt_node_t*   parent;
    nodePosition sub_nodePosition;
    nodePosition nodePosition;

    BLOG ( "In btree_delete_subtree\n" );

    node   = subtree;
    parent = NULL;

del_loop:

    //
    //  Starting at node "subtree" (x), test whether the key k is in the node.
    //  If it is, descend to the delete cases. If it's not, prepare to descend
    //  into a subtree, but ensure that it has at least t (btree->minDegree) keys,
    //  so that we can delete 1 key without promoting a new root.
    //
    //  Remember: the minimum number of keys in a (non-root) node is
    //            btree->min (equivalently, (t - 1) or splitPoint)
    //
    while ( true )
    {
        //
        //  If there are no keys in this node, return an error
        //
        if ( ! node->keysInUse )
        {
            return -ENODATA;
        }
        //
        //  Find the index of the key greater than or equal to the key that we
        //  are looking for. If our target key is greater than all of the keys
        //  in use in this node, i == node->keysInUse and we descend into the
        //  right child of the largest key in the node
        //
        i = 0;
        while ( i < node->keysInUse )
        {
            diff = btree_compare_function ( btree, data,
                                            getDataRecord ( btree, node, i ) );
            //
            //  If we found a key that is less than (or equal to) the one we
            //  are looking for, just break out of
            //  the loop.
            //
            if ( diff <= 0 )
            {
                break;
            }
            i++;
        }
        //
        //  Save the index value of this location.
        //
        index = i;

        //
        //  If we found the exact key we are looking for, just break out of
        //  the loop.
        //
        if ( diff == 0 )
        {
            break;
        }
        //
        //  If we are in a leaf node and we didn't find the key in question
        //  then we have a "not found" condition.
        //
        if ( isLeaf ( node )  )
        {
            return -ENODATA;
        }
        //
        //  Save the current node as the parent.
        //
        parent = node;

        //
        //  Get the left child of the key that we found -- since we stopped at
        //  the first record greater than our key, the key must be in the
        //  left subtree.
        //
        //  Note that we use the "getChildPtr" function here because "i" may
        //  have already been incremented past the record we found.
        //  TODO: i is different because of the different exits from the loop.
        node = getChildPtr ( btree, node, i );

        //
        //  If the child pointer is NULL then we have a "not found" condition.
        //
        if ( node == NULL )
        {
            return -ENODATA;
        }
        //
        //  If the node has at least (splitPoint + 1) (aka, t) keys, descend
        //  directly into the subtree rooted at the new node to search for the
        //  key k.
        //
        //  if ( node->keysInUse > splitPoint )
        if ( node->keysInUse > splitPoint )
        {
            continue;
        }
        //
        //  Since the node does not have at least t keys, we cannot safely
        //  descend into the subtree rooted at the node. Instead, merge the
        //  node with an immediate sibling. Perform some tests to figure out
        //  exactly how we should do this.
        //
        //  These cases correspond to case "3" from Cormen, et al., 2009
        //  (p501)
        //
        //  If we are at the last populated position in the parent node...
        //
        if ( index == ( parent->keysInUse ) )
        {
            //
            //  Set the left sibling to the left child of our parent and the
            //  right sibling is NULL.
            //
            lsibling = getChildPtr ( btree, parent, parent->keysInUse - 1 );
            rsibling = NULL;
        }
        //
        //  Otherwise, if we are at the beginning of the node, set the left
        //  sibling to NULL and the right sibling to the right child of our
        //  parent.
        //
        else if ( index == 0 )
        {
            lsibling = NULL;
            rsibling = getChildPtr ( btree, parent, 1 );
        }
        //
        //  Otherwise, we are somewhere in the middle of the node so just set
        //  the left and right siblings to the left and right children of our
        //  parent.
        //
        else
        {
            lsibling = getChildPtr ( btree, parent, i - 1 );
            rsibling = getChildPtr ( btree, parent, i + 1 );
        }
        //
        //  Now that we have the right and left children of the parent record
        //  we need to collapse the 2 children and the parent record into one
        //  of the children...
        //
        if ( node->keysInUse == splitPoint && parent )
        {
            BLOG ( "In btree_delete_subtree[3a]\n" );

            //
            //  Case 3a: If the target node has (t - 1) keys but the right
            //           sibling has t keys. Give the target node an extra key
            //           by moving a key from the parent into the target, then
            //           move a key from the target node's left sibling back
            //           into the parent, and update the corresponding
            //           pointers.
            //
            if ( rsibling && ( rsibling->keysInUse > splitPoint ) )
            {
                move_key ( btree, parent, i, left );
            }
            //
            //  Case 3a: If the target node has (t - 1) keys but the left
            //           sibling has t keys. Just like the previous case, but
            //           move a key from the target node's right sibling into
            //           the parent.
            //
            else if ( lsibling && ( lsibling->keysInUse > splitPoint ) )
            {
                move_key ( btree, parent, i, right );
            }
            //
            //  Case 3b -- merge LEFT sibling
            //
            //          The target node and its siblings have (t - 1) keys, so
            //          pick a sibling to merge. Move a key from the parent
            //          into the merged node to serve as the new median key.
            //
            else if ( lsibling && ( lsibling->keysInUse == splitPoint ) )
            {
                BLOG ( "In btree_delete_subtree[3b1]\n" );

                node = merge_siblings ( btree, parent, i );
            }
            //
            //  Case 3b -- merge RIGHT sibling
            //
            else if ( rsibling && ( rsibling->keysInUse == splitPoint ) )
            {
                BLOG ( "In btree_delete_subtree[3b2]\n" );

                node = merge_siblings ( btree, parent, i );
            }
            //
            //  The current node doesn't have enough keys, but we can't
            //  perform a merge, so return an error
            //
            else
            {
                BLOG ( "In btree_delete_subtree[3b3]\n" );

                return -1;
            }
        }
    }
    //
    //  Case 1 : If the node containing the key is found and is a leaf node.
    //           The leaf node also has keys greater than the minimum required
    //           so we simply remove the key.
    //
    if ( isLeaf ( node )  && ( node->keysInUse > splitPoint ) )
    {
        BLOG ( "In btree_delete_subtree[1a]\n" );

        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );

        //
        //  Decrement the number of records in the btree.
        //
        --btree->count;

        return 0;
    }
    //
    //  If the node is the root permit deletion even if the number of
    //  keys is less than "min" (t - 1).
    //
    if ( isLeaf ( node )  && ( node == cvtToAddr ( btree, btree->root ) ) )
    {
        BLOG ( "In btree_delete_subtree[1a]\n" );

        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );

        //
        //  Decrement the number of records in the btree.
        //
        --btree->count;

        return 0;
    }
    //
    //  Case 2: If the node containing the key we are looing for was found and
    //          is an internal node.
    //
    if ( ! isLeaf ( node )  )
    {
        //
        //  Case 2a: If the left child of our record has at least the
        //           splitPoint number of records (otherwise we can't delete
        //           from it)...
        //
        if ( getLeftChild ( btree, node, index )->keysInUse > splitPoint )
        {
            BLOG ( "In btree_delete_subtree[2a]\n" );

            //
            //  Go find the highest key in this left subtree...
            //
            sub_nodePosition =
                get_max_key_pos ( btree, getLeftChild ( btree, node, index ) );

            //
            //  Replace the record that we are deleting with the highest key
            //  in the right subtree.
            //
            copyRecord ( btree, sub_nodePosition.node, sub_nodePosition.index,
                         node, index );
            //
            //  Go delete the record that we just moved up into our node.
            //
            btree_delete_subtree ( btree, getLeftChild ( btree, node, index ),
                                   getDataAddress ( btree, node, index) );
            //
            //  If the node we found is not a leaf, issue and error message.
            //  TODO: Note that this should be a more meaningful message!
            //
            if ( ! isLeaf ( sub_nodePosition.node ) )
            {
                printf ( "Error: Not leaf\n" );
            }
        }
        //
        // Case 2b: If the right child of our record has at least the
        //          splitPoint number of records (otherwise we can't delete
        //          from it)...
        //
        else if ( getRightChild ( btree, node, index )->keysInUse > splitPoint )
        {
            BLOG ( "In btree_delete_subtree[2b]\n" );

            //
            //  Go find the smallest key in the right subtree...
            //
            sub_nodePosition =
                get_min_key_pos ( btree, getRightChild ( btree, node, index ) );
            //
            //  Replace the target record in our node with the smallest key in
            //  the subtree that we found.
            //
            copyRecord ( btree, sub_nodePosition.node, sub_nodePosition.index,
                         node, index );
            //
            //  Go delete the record that we just moved up into our node.
            //
            btree_delete_subtree ( btree, getRightChild ( btree, node, index ),
                                   getDataRecord ( btree, node, index ) );
            //
            //  If the node we found is not a leaf, issue and error message.
            //  TODO: Note that this should be a more meaningful message!
            //
            if ( ! isLeaf ( sub_nodePosition.node ) )
            {
                printf ( "Error: Not leaf\n" );
            }
        }
        //
        // Case 2c: both y and z have (t - 1) keys, so merge k and z into y, so
        //          x loses both k and the pointer to z, and y now contains
        //          (2t - 1) keys. Then free z, and recursively delete k from y
        //
        else if ( getLeftChild ( btree, node, index )->keysInUse == splitPoint &&
                  getLeftChild ( btree, node, index + 1 )->keysInUse == splitPoint )
        {
            BLOG ( "In btree_delete_subtree[2c]\n" );

            //
            //  Go merge the siblings of this node at the specified index
            //  (which will remove the data record at node->dataRecords[index]
            //  from this node as well).
            //
            node = merge_siblings ( btree, node, index );

            goto del_loop;
        }
    }
    //
    //  Case 3: In this case start from the top of the tree and continue
    //          moving to the leaf node making sure that each node that we
    //          encounter on the way has at least 't' (minDegree of the tree) keys
    //
    if ( isLeaf ( node )  && ( node->keysInUse > splitPoint ) )
    {
        BLOG ( "In btree_delete_subtree[3]\n" );

        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );
    }
    //
    //  All done so return to the caller.
    //
    return 0;
}


//
//  Define the user space function that deletes from a btree.
//
int btree_delete ( btree_t* btree, void* data )
{
    BLOG ( "In btree_delete\n" );

    int returnCode = 0;

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go actually delete the given record from the tree.
    //
    returnCode = btree_delete_subtree ( btree, cvtToAddr ( btree, btree->root ),
                                        data );
    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    return returnCode;
}


/**
*   Function used to get the node containing the given key
*   @param btree The btree to be searched
*   @param key The the key to be searched
*   @return The node and position of the key within the node
*/
nodePosition get_btree_node ( btree_t* btree, void* key )
{
    nodePosition nodePosition = { 0, 0 };
    bt_node_t*   node;
    unsigned int i;
    int          diff;

    BLOG ( "In get_btree_node\n" );

    //
    //  Start at the root of the tree...
    //
    node = cvtToAddr ( btree, btree->root );

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  Initialize our position in this node to the first record of the
        //  node.
        //
        i = 0;

        //
        //  Search the records in this node beginning from the start until
        //  we find a record that is equal to or larger than the target...
        //
        while ( i < node->keysInUse )
        {
            //
            //  Go compare the target with the current data record.
            //
            diff = btree_compare_function ( btree, key,
                                            getDataRecord ( btree, node, i) );
            //
            //  If the comparison results in a value < 0 then we are done so
            //  just break out of this loop.
            //
            if ( diff < 0 )
            {
                break;
            }
            //
            //  If the record that we stopped on matches our target then return it
            //  to the caller.
            //
            if ( diff == 0 )
            {
                nodePosition.node  = node;
                nodePosition.index = i;
                return nodePosition;
            }
            i++;
        }
        //
        //  If the node is a leaf and if we did not find the target in it then
        //  return a NULL position to the caller for a "not found" indicator.
        //
        if ( isLeaf ( node )  )
        {
            return nodePosition;
        }
        //
        //  Get the left child node and go up and try again in this node.
        //
        node = getLeftChild ( btree, node, i );
    }
    //
    //  This point in the code should never be hit!
    //
    return nodePosition;
}


/**
*       Used to destory btree
*       @param btree The B-tree
*       @return none
*/
void btree_destroy ( btree_t* btree )
{
    int i = 0;

    return;     // TODO: Temp fix for address problem!

    BLOG ( "In btree_destroy\n" );

    bt_node_t* head;
    bt_node_t* tail;
    bt_node_t* child;
    bt_node_t* del_node;

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Start at the root node...
    //
    head = tail = cvtToAddr ( btree, btree->root );

    //
    //  Repeat until the head pointer becomes NULL...
    //
    while ( head != NULL )
    {
        //
        //  If this is not a leaf node...
        //
        if ( ! isLeaf ( head ) )
        {
            //
            //  For each record in this node...
            //
            //  ??? I think this is chaining all of the nodes into a list
            //  using the "next" pointer in each node before actually deleting
            //  any nodes.  I'm not sure if this is needed - Can't we just
            //  traverse the tree depth first and delete the nodes as we go?
            //
            for ( i = 0; i < head->keysInUse + 1; i++ )
            {
                child = getLeftChild ( btree, head, i );
                tail->next = cvtToOffset ( btree, child );
                tail = child;
                child->next = 0;
            }
        }
        //
        //  Grab the current list head for the delete that follows and then
        //  update the head to the next node in the list.
        //
        del_node = head;
        head = cvtToAddr ( btree, head->next );

        //
        //  Go delete the node that we saved above.
        //
        free_btree_node ( btree, del_node );
    }
    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    return;
}

/**
*       Function used to search a node in a B-Tree
*       @param btree The B-tree to be searched
*       @param key Key of the node to be search
*       @return The address of the user's data record
*/
void* btree_search ( btree_t* btree, void* key )
{
    BLOG ( "In btree_search\n" );

    //
    //  Initialize the return data.
    //
    void* data = NULL;

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go find the given key in the tree...
    //
    nodePosition nodePos = get_btree_node ( btree, key );

    //
    //  If the target data was found in the tree, get the pointer to the data
    //  record to return to the caller.
    //
    if ( nodePos.node )
    {
        data = getDataRecord ( btree, nodePos.node, nodePos.index );
    }
    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    //
    //  Return whatever we found to the caller.
    //
    return data;
}


/*!----------------------------------------------------------------------------

    b t r e e _ g e t _ m a x

    @brief Get the maximum user data value in the btree.

    This function will return a pointer to the user data record contained in
    the largest value in the btree.  The "largest" is defined according to
    the user supplied comparison function.

    If the btree is empty, a null pointer will be returned.

    @param[in] btree - The address of the btree to search

    @return The address of the maximum value in the btree.

-----------------------------------------------------------------------------*/
void* btree_get_max ( btree_t* btree )
{
    nodePosition nodePosition;
    void*        returnValue;

    BLOG ( "In btree_get_max\n" );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    nodePosition = get_max_key_pos ( btree, cvtToAddr ( btree, btree->root ) );
    if ( nodePosition.node == NULL )
    {
        returnValue = NULL;
    }
    else
    {
        returnValue = getDataRecord ( btree, nodePosition.node, nodePosition.index );
    }
    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    return returnValue;
}


/*!----------------------------------------------------------------------------

    b t r e e _ g e t _ m i n

    @brief Get the minimum user data value in the btree.

    This function will return a pointer to the user data record contained in
    the smallest value in the btree.  The "smallest" is defined according to
    the user supplied comparison function.

    If the btree is empty, a null pointer will be returned.

    @param[in] btree - The address of the btree to search

    @return The address of the minimum value in the btree.

-----------------------------------------------------------------------------*/
void* btree_get_min ( btree_t* btree )
{
    nodePosition nodePosition;
    void*        returnValue;

    BLOG ( "In btree_get_min\n" );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    nodePosition = get_min_key_pos ( btree, cvtToAddr ( btree, btree->root ) );
    if ( nodePosition.node == 0 )
    {
        returnValue = NULL;
    }
    else
    {
        returnValue = getDataRecord ( btree, nodePosition.node, nodePosition.index );
    }
    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    return returnValue;
}


//
//  This function will traverse the btree supplied and print all of the data
//  records in it in order starting with the minimum record.  This is
//  essentially a "dump" of the contents of the given btree subtree.
//
//  This function will be called by the "btree_traverse" API function with the
//  root node of the specified btree and then it will call itself recursively
//  to walk the tree.
//
void btree_traverse_node ( btree_t*     btree,
                           bt_node_t*   subtree,
                           traverseFunc traverseCB )
{
    int        i;
    bt_node_t* node = subtree;

    BLOG ( "In btree_traverse_node\n" );

    //
    //  If this is not a leaf node...
    //
    if ( ! isLeaf ( node )  )
    {
        //
        //  Loop through all of the sub nodes of the current node and process
        //  each subtree.
        //
        for ( i = 0; i < node->keysInUse; ++i )
        {
            //
            //  Go process all of the nodes in the left subtree of this node.
            //
            btree_traverse_node ( btree, getLeftChild ( btree, node, i ),
                                  traverseCB );
            //
            //  Once the left subtree has been finished, display the current
            //  data record in this node.
            //
            traverseCB ( "", getDataRecord ( btree, node, i ) );
        }
        //
        //  Now that we have traversed the left subtree and the current node
        //  entry, go process the right subtree of this record in this node.
        //
        //  Note that we are calling "getLeftChild" here because "i" has
        //  already been incremented to the right pointer so we don't want to
        //  access the "i + 1" index that the right child would normally have.
        //
        btree_traverse_node ( btree, getLeftChild ( btree, node, i ),
                              traverseCB );
    }
    //
    //  Otherwise, if this is a leaf node...
    //
    else
    {
        //
        //  Loop through all of the data records defined in this node and
        //  print them using the print callback function supplied by the user.
        //
        for ( i = 0; i < node->keysInUse; ++i )
        {
            traverseCB ( "", getDataRecord ( btree, node, i ) );
        }
    }
    return;
}


//
//  This is the user-facing API function that will walk the entire btree
//  supplied by the caller starting with the root node.  As each data record
//  is found in the tree, the user's supplied callback function will be invoked
//  with the pointer to the user data record that was just found.
//
//  The btree given will be walked in minDegree starting with the data record that
//  has the minimum value until the entire tree has been processed.
//
void btree_traverse ( btree_t* btree, traverseFunc traverseCB )
{
    BLOG ( "In btree_traverse\n" );

    //
    //  if the caller did not supply a traverse function, we can't do anything
    //  meaningful so just exit.
    //
    if ( traverseCB == NULL )
    {
        return;
    }
    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go traverse this btree starting from the root node.
    //
    btree_traverse_node ( btree, cvtToAddr ( btree, btree->root ), traverseCB );

    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    B t r e e   I t e r a t i o n   F u n c t i o n s

    This section contains the implementation of the btree iterator functions.

    Example usage (assuming the btree is populated with "userData" records):

        userData record = { 12, "" };

        btree_iter iter = btree_find ( &record );

        while ( ! btree_iter_at_end ( iter ) )
        {
            userData* returnedData = btree_iter_data ( iter );
            ...
            btree_iter_next ( iter );
        }
        btree_iter_cleanup ( iter );

    The above example will find all of the records in the btree beginning with
    { 12, "" } to the end of the btree.

-----------------------------------------------------------------------------*/


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r a t o r _ n e w

    @brief Allocate and initialize a new iterator object.

    This function will allocate memory for a new iterator object and
    initialize the fields in that object appropriately.

    Note: The memory for this iterator is allocated with malloc rather than
    the shared memory allocator because it will live in the user's address
    space and is never seen by another process.

    @param[in] btree - The address of the btree object to be operated on.
    @param[in] key - The address of the user's data object to be found in the
                     tree.

    @return The btree_iter object

-----------------------------------------------------------------------------*/
static btree_iter btree_iterator_new ( btree_t* btree, void* key )
{
    BLOG ( "In btree_iterator_new: btree[%p], key[%p]\n", btree, key );

    //
    //  Allocate a new iterator that we will return to the caller.
    //
    btree_iter iter = malloc ( sizeof(btree_iterator_t) );

    //
    //  If the memory allocation succeeded...
    //
    if ( iter != NULL )
    {
        //
        //  Initialize the fields of the iterator.
        //
        iter->btree = btree;
        iter->key   = key;
        iter->node  = 0;
        iter->index = 0;
    }
    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ f i n d

    @brief Position an iterator at the specified key location.

    The btree_find function is similar to the btree_search function except
    that it will find the smallest key that is greater than or equal to the
    specified value (rather than just the key that is equal to the specified
    value).

    This function is designed to be used for forward iterations using the
    iter->next function to initialize the user's iterator to the beginning of
    the forward range of values the user wishes to find.

    If the caller attempts to position the iterator past the end of the given
    btree, a null iterator will be returned.

    The iterator returned must be disposed of when the user has finished
    with it by calling the btree_iter_cleanup function.  If this is not done,
    there will be a memory leak in the user's program.

    @param[in] btree - The address of the btree object to be operated on.
    @param[in] key - The address of the user's data object to be found in the
                     tree.

    @return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_find ( btree_t* btree, void* key )
{
    btree_iter iter;

    BLOG ( "In btree_find: btree[%p], key[%p]\n", btree, key );

    PRINT_BTREE ( btree, NULL );

    bt_node_t* node  = 0;
    int        diff  = 0;
    int        i     = 0;
    int        index = 0;

    PRINT_DATA ( "  Looking up key:  ", key );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go allocate and initialize a new iterator object.
    //
    iter = btree_iterator_new ( btree, key );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    if ( iter == NULL )
    {
        printf ( "Error: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        goto findExit;
    }
    //
    //  Start at the root of the tree...
    //
    node = cvtToAddr ( btree, btree->root );

    //
    //  Examine the current node...
    //
    while ( true )
    {
        //
        //  Initialize our position in this node to the first record of the
        //  node.  We will be searching the node from beginning to end.
        //
        i = 0;

        PRINT_DATA ( "  Current node is: ", getDataRecord ( btree, node, i ) );

        //
        //  Search the records in this node beginning from the start until we
        //  find a record that is less than the target...
        //
        while ( i < node->keysInUse )
        {
            //
            //  Save this node index for later processing.  Note that we do
            //  this because the value of "i" may or may not have been
            //  incremented past the end of the records array at the end of
            //  this loop depending on whether the loop terminates or we take
            //  an early out via the break condition.
            //
            index = i;

            //
            //  Go compare the target with the current data record and get the
            //  difference.  Note that this diff value cannot be used as a
            //  numeric "goodness" indicator because it is dependent on how
            //  the user has defined the comparison operator for this btree.
            //  The only thing that can be tested is the sign of this value.
            //
            diff = btree_compare_function ( btree, key,
                                            getDataRecord ( btree, node, i ) );

            BLOG ( "  Searching node at %d, diff: %d\n", i, diff );

            //
            //  If the record that we are looking at is less than or equal to
            //  our target then quit looking in this node.
            //
            if ( diff <= 0 )
            {
                break;
            }
            //
            //  Increment our index position and continue.
            //
            ++i;
        }
        //
        //  If we found an exact match to our key, just stop and return the
        //  iterator to the caller.
        //
        if ( diff == 0 )
        {
            BLOG ( "  Diff == 0 - Returning node at i = %d\n", index );

            //
            //  Save the node and index where the match occurred in the
            //  iterator and terminate any further searching.
            //
            iter->node  = node;
            iter->index = index;

            break;
        }
        //
        //  If the difference is positive, we are going to have to go down the
        //  right subtree but we need to save the current position first.
        //
        else if ( diff > 0 )
        {
            //
            //  If this is a leaf node then we are at the end of the tree so
            //  just return the current iterator to the caller.
            //
            if ( isLeaf ( node )  )
            {
                BLOG ( "  Diff > 0 - Leaf - Returning node at i = %d\n", index );

                break;
            }
            //
            //  If this is not a leaf node, we will need to continue our
            //  search with the right child node of the current position.
            //
            //  If the comparison results in a diff value > 0 then the value
            //  we want might be in the right child so get the right child and
            //  continue our search.

            //
            else
            {
                BLOG ( "  Diff > 0 - Not leaf - Moving to right child.\n" );
                PRINT_NODE ( btree, node );

                node = getRightChild ( btree, node, index );
            }
        }
        //
        //  Otherwise, if the difference is negative, we are going to have to
        //  go down the left subtree but we need to save the current position
        //  first.
        //
        else   // if ( diff < 0 )
        {
            //
            //  If this is a leaf node, then we are at the end of the tree so
            //  save the current node position in the iterator and stop our
            //  search.
            //
            if ( isLeaf ( node )  )
            {
                BLOG ( "  Diff < 0 - Leaf - Returning node at i = %d\n", index );

                iter->node  = node;
                iter->index = index;

                break;
            }
            //
            //  If this is not a leaf node, we will need to continue our
            //  search with the left child node of the current position.
            //
            //  since the comparison results in a diff value < 0 then the
            //  value we want must be in the left child, so save the current
            //  position (in case nothing in the left child is less than what
            //  we found so far) get the left child and continue our search.
            //
            else
            {
                BLOG ( "  Diff < 0 - Not leaf - Moving to left child.\n" );

                iter->node  = node;
                iter->index = index;

                node = getLeftChild ( btree, node, index );
            }
        }
    }
    //
    //  We have finished our search.  If nothing in the tree is less than or
    //  equal to our target value then we need to return an "end" condition to
    //  the caller by returning a NULL iterator.
    //
    if ( iter->node == 0 )
    {
        BLOG ( "  Found iterator end()\n" );

        //
        //  Go free up the memory allocated to our iterator since we won't be
        //  returning that value to the caller.
        //
        free ( iter );
        iter = NULL;
    }
    //
    //  If we have a valid iterator...
    //
    else
    {
        BLOG ( "  Found index %d:\n", iter->index );
        PRINT_NODE ( btree, iter->node );

        //
        //  Update the key value in the iterator to the record that we
        //  actually found.
        //
        iter->key = getDataRecord ( btree, iter->node, iter->index );
    }
findExit:

    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ r f i n d

    @brief Position an iterator at the specified key location.

    The btree_rfind function is similar to the btree_search function except
    that it will find the largest key that is less than or equal to the
    specified value (rather than just the key that is equal to the specified
    value).

    This function is designed to be used for reverse iterations using the
    iter->previous function to initialize the user's iterator to the beginning
    of the reverse range of values the user wishes to find.

    If the caller attempts to position the iterator past the beginning of the
    given btree, a null iterator will be returned.

    The iterator returned must be disposed of when the user has finished
    with it by calling the btree_iter_cleanup function.  If this is not done,
    there will be a memory leak in the user's program.

    @param[in] btree - The address of the btree object to be operated on.
    @param[in] key - The address of the user's data object to be found in the
                     tree.

    @return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_rfind ( btree_t* btree, void* key )
{
    btree_iter iter;

    BLOG ( "In btree_rfind: btree[%p], key[%p]\n", btree, key );

    bt_node_t* node  = 0;
    int        diff  = 0;
    int        i     = 0;
    int        index = 0;

    PRINT_DATA ( "  Looking up key:  ", key );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go allocate and initialize a new iterator object.
    //
    iter = btree_iterator_new ( btree, key );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    //  TODO: Get rid of the goto by making the body of the function the
    //  "else" clause.
    //
    if ( iter == NULL )
    {
        errno = ENOMEM;
        printf ( "Error: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        goto rfindExit;
    }
    //
    //  Start at the root of the tree...
    //
    node = cvtToAddr ( btree, btree->root );

    //
    //  Examine the current node...
    //
    while ( true )
    {
        //
        //  Initialize our position in this node to the last record of the
        //  node.  We will be searching the node backward from the end to the
        //  beginning.
        //
        i = node->keysInUse - 1;

        PRINT_DATA ( "  Current node is: ", getDataRecord ( btree, node, i ) );

        //
        //  Search the records in this node backwards from the end until
        //  we find a record that is less than the target...
        //
        while ( i >= 0 )
        {
            //
            //  Save this node index for later processing.  Note that we do
            //  this because the value of "i" may or may not have been
            //  decremented past the beginning of the records array when this
            //  loop terminates depending on whether the loop terminates or we
            //  take an early out via the break condition.
            //
            index = i;

            //
            //  Go compare the target with the current data record and get the
            //  difference.  Note that this diff value cannot be used as a
            //  numeric "goodness" indicator because it is dependent on how
            //  the user has defined the comparison operator for this btree.
            //  The only thing that can be tested is the sign of this value.
            //
            diff = btree_compare_function ( btree, key,
                                            getDataRecord ( btree, node, i ) );

            BLOG ( "  Searching node at %d, diff: %d\n", i, diff );

            //
            //  If the record that we are looking at is greater than or equal
            //  to our target then quit looking in this node.
            //
            if ( diff >= 0 )
            {
                break;
            }
            //
            //  Decrement our index position and continue.
            //
            //  TODO: Try moving this dec up before the prev. test - That
            //  might eliminate the need for the index variable.
            //
            --i;
        }
        //
        //  If we found an exact match to our key, just stop and return the
        //  iterator to the caller.
        //
        if ( diff == 0 )
        {
            BLOG ( "  Diff == 0 - Returning node at i = %d\n", index );

            //
            //  Save the node and index where the match occurred in the
            //  iterator and terminate any further searching.
            //
            iter->node  = node;
            iter->index = index;

            break;
        }
        //
        //  If the difference is positive, we are going to have to go down the
        //  right subtree but we need to save the current position first.
        //
        else if ( diff < 0 )
        {
            //
            //  If this is a leaf node then we are at the end of the tree so
            //  just return the current iterator to the caller.
            //
            if ( isLeaf ( node )  )
            {
                BLOG ( "  Diff > 0 - Leaf - Returning node at i = %d\n", index );

                break;
            }
            //
            //  If this is not a leaf node, we will need to continue our
            //  search with the left child node of the current position.
            //
            //  If the comparison results in a diff value > 0 then the value
            //  we want might be in the left child so get the left child and
            //  continue our search.
            //
            else
            {
                BLOG ( "  Diff > 0 - Not leaf - Moving to left child.\n" );
                PRINT_NODE ( btree, node );

                node = getLeftChild ( btree, node, index );
            }
        }
        //
        //  Otherwise, if ther difference is negative, we are going to have to
        //  go down the left subtree but we need to save the current position
        //  first.
        //
        else   // if ( diff > 0 )
        {
            //
            //  If this is a leaf node, then we are at the end of the tree so
            //  save the current node position in the iterator and stop our
            //  search.
            //
            if ( isLeaf ( node )  )
            {
                BLOG ( "  Diff < 0 - Leaf - Returning node at i = %d\n", index );

                iter->node  = node;
                iter->index = index;

                break;
            }
            //
            //  If this is not a leaf node, we will need to continue our
            //  search with the right child node of the current position.
            //
            //  If the comparison results in a diff value < 0 then the value
            //  we want might be in the right child so save the current
            //  position (in case nothing in the right child is less than what
            //  we found so far) get the right child and continue our search.
            //
            else
            {
                BLOG ( "  Diff < 0 - Not leaf - Moving to left child.\n" );

                iter->node  = node;
                iter->index = index;

                node = getRightChild ( btree, node, index );
            }
        }
    }
    //
    //  We have finished our search.  If nothing in the tree is less than or
    //  equal to our target value then we need to return an "end" condition to
    //  the caller by returning a NULL iterator.
    //
    if ( iter->node == 0 )
    {
        BLOG ( "  Found iterator end()\n" );

        //
        //  Go free up the memory allocated to our iterator since we won't be
        //  returning that value to the caller.
        //
        free ( iter );
        iter = NULL;
    }
    //
    //  If we have a valid iterator...
    //
    else
    {
        BLOG ( "  Found index %d:\n", iter->index );
        PRINT_NODE ( btree, iter->node );

        //
        //  Update the key value in the iterator to the record that we
        //  actually found.
        //
        iter->key = getDataRecord ( btree, iter->node, iter->index );
    }
rfindExit:

    //
    //  Release the btree data structure lock.
    //
    BTREE_UNLOCK

    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ b e g i n

    @brief Create an iterator positioned at the beginning of the btree.

    This function will find the smallest record currently defined in the
    specified btree and return an iterator to that record.  If the btree is
    empty, an iterator will be returned but it will be empty.  This iterator
    can be compared to the iter->end() iterator to determine if it empty.

    This function will return a new iterator object which the user MUST
    destroy (via btree_iter_cleanup()) when he is finished with it.

    @param[in] btree - The address of the btree object to be operated on.

    @return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_iter_begin ( btree_t* btree )
{
    btree_iter   iter;
    nodePosition nodePosition;

    BLOG ( "In btree_iter_begin\n" );

    //
    //  If the given btree is empty, return a null object.
    //
    if ( btree->count == 0 )
    {
        return NULL;
    }
    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go allocate and initialize a new iterator object.
    //
    iter = btree_iterator_new ( btree, 0 );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    if ( iter == NULL )
    {
        errno = ENOMEM;
        printf ( "Error: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        goto beginExit;
    }
    //
    //  Go get the minimum key currently defined in the btree.
    //
    nodePosition = get_min_key_pos ( btree, cvtToAddr ( btree, btree->root ) );

    //
    //  Save the tree position in the new iterator.
    //
    iter->index = nodePosition.index;
    iter->node  = nodePosition.node;
    iter->key   = getDataRecord ( btree, nodePosition.node, nodePosition.index );

    BLOG ( "  Found minimum record at index %d in node %p\n",
           nodePosition.index, nodePosition.node );

    PRINT_NODE ( btree, nodePosition.node );

    //
    //  Release the btree data structure lock.
    //
beginExit:

    BTREE_UNLOCK

    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ e n d

    @brief Create an iterator positioned past the last record of the btree.

    This function will create a new iterator and set it to indicate that is is
    an "end" iterator.

    This function will return a new iterator object which the user MUST
    destroy (via btree_iter_cleanup()) when he is finished with it.

    @param[in] btree - The address of the btree object to be operated on.

    @return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_iter_end ( btree_t* btree )
{
    btree_iter   iter;
    nodePosition nodePosition;

    BLOG ( "In btree_iter_end\n" );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Go allocate and initialize a new iterator object.
    //
    iter = btree_iterator_new ( btree, 0 );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    if ( iter == NULL )
    {
        errno = ENOMEM;
        printf ( "Error: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        goto endExit;
    }
    //
    //  Go get the maximum key currently defined in the btree.
    //
    nodePosition = get_max_key_pos ( btree, cvtToAddr ( btree, btree->root ) );

    //
    //  Save the tree position in the new iterator.
    //
    iter->index = nodePosition.index;
    iter->node  = nodePosition.node;
    iter->key   = getDataRecord ( btree, nodePosition.node, nodePosition.index );

    BLOG ( "  Found maximum record at index %d in node %p\n",
           nodePosition.index, nodePosition.node );

    PRINT_NODE ( btree, nodePosition.node );

    //
    //  Release the btree data structure lock.
    //
endExit:

    BTREE_UNLOCK

    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ n e x t

    @brief Get the "next" position in the btree.

    This function will position the specified iterator to the next higher key
    value in the btree from the current position.

    This function will use the position defined in the supplied iterator as
    the starting point in the btree and proceed to move to the "next" higher
    record in the btree.  Note that "next" is defined by the user's supplied
    comparison operator for this btree.

    @param[in,out] iter - The iterator to be updated

    @return None

-----------------------------------------------------------------------------*/
void btree_iter_next ( btree_iter iter )
{
    BLOG ( "In btree_iter_next: node[%p], index[%d]\n", iter->node, iter->index );

    //
    //  Set the internal variables so that we will start searching where the
    //  current iterator points.
    //
    btree_t*     btree = iter->btree;
    bt_node_t*   node  = iter->node;
    void*        key   = iter->key;
    void*        dataValue;
    int          diff  = 0;
    int          i     = iter->index;
    int          index = 0;
    nodePosition nodePos;

    //
    //  Print out the target key that we are starting at.
    //
    PRINT_DATA ( "  Looking up key:  ", key );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  If this is a leaf node...
        //
        if ( isLeaf ( node )  )
        {
            //
            //  Loop through the records in this node until we reach the end
            //  of the node...
            //
            i = 0;
            PRINT_DATA ( "  Current leaf node is: ",
                         getDataRecord ( btree, node, i ) );

            while ( i < node->keysInUse )
            {
                //
                //  Compare the current record with our target.
                //
                index = i;
                diff = btree_compare_function ( btree, key,
                                                getDataRecord ( btree, node, i ) );

                BLOG ( "  Searching node at %d, diff: %d\n", i, diff );

                //
                //  If the current record is larger than the target we have
                //  found the "next" record in the btree so return it to the
                //  caller.
                //
                if ( diff < 0 )
                {
                    BLOG ( "  Target record found.\n" );
                    iter->index = i;
                    iter->node  = node;
                    iter->key   = getDataRecord ( btree, node, i );
                    goto nextExit;
                }
                ++i;
            }
            //
            //  If none of the records in this node are larger than the
            //  target, go up one level in the btree to the parent node and
            //  try again.
            //
            //  If we are not at the root node...
            //
            if ( node->parent != 0 )
            {
                BLOG ( "  Moving up to parent.\n" );
                i    = 0;
                node = cvtToAddr ( btree, node->parent );
            }
            //
            //  If we are at the root node then the target is larger than the
            //  largest record in the btree so return an "end" condition to
            //  the caller.
            //
            else
            {
                BLOG ( "  End of tree found.\n" );
                iter->index = -1;
                iter->node  = 0;
                iter->key   = 0;
                goto nextExit;
            }
            //
            //  Go back up and start looking at the records in the node...
            //
            continue;
        }
        //
        //  If the current node is an interior node (i.e. not a leaf node)...
        //
        i = 0;
        PRINT_DATA ( "  Current interior node is: ",
                     getDataRecord ( btree, node, i ) );
        //
        //  Examine all of the records until we reach the end of the records
        //  in this node.
        //
        while ( i < node->keysInUse )
        {
            //
            //  Compare the current record with the target record.
            //
            index = i;
            diff = btree_compare_function ( btree, key,
                                            getDataRecord ( btree, node, i ) );

            BLOG ( "  Searching node at %d, diff: %d\n", i, diff );

            //
            //  If the current record is larger than the target...
            //
            if ( diff < 0 )
            {
                //
                //  Go find the smallest record in the left subtree of this
                //  record.
                //
                BLOG ( "  Searching for minimum subtree key in left child.\n" );
                nodePos = get_min_key_pos ( btree,
                                            getLeftChild ( btree, node, i ) );
                //
                //  If this minimum value is larger than the target then that's
                //  the "next" record.
                //
                dataValue = getDataRecord ( btree, nodePos.node, nodePos.index );
                diff = btree_compare_function ( btree, key, dataValue );
                if ( diff < 0 )
                {
                    //
                    //  Return this record to the caller.
                    //
                    PRINT_DATA ( "    Found: ",
                                 getDataRecord ( btree, nodePos.node, nodePos.index) );
                    iter->index = nodePos.index;
                    iter->node  = nodePos.node;
                    iter->key   = dataValue;
                    goto nextExit;
                }
                //
                //  If the left subtree doesn't have the record we want then
                //  it must be the current record so return that to the
                //  caller.
                //
                else
                {
                    BLOG ( "  Target record found.\n" );
                    iter->index = i;
                    iter->node  = node;
                    iter->key   = getDataRecord ( btree, node, i );
                    goto nextExit;
                }
            }
            ++i;
        }
        //
        //  If we have just exhausted the list of records in the current
        //  node...
        //
        if ( i == node->keysInUse )
        {
            //
            //  We need to decide if we need to go up or down the btree from
            //  here so find the largest record in the right subtree.
            //
            nodePos = get_max_key_pos ( btree, getLeftChild ( btree, node, i ) );
            dataValue = getDataRecord ( btree, nodePos.node, nodePos.index );
            diff = btree_compare_function ( btree, key, dataValue );

            //
            //  If there are no records in the right subtree that are larger
            //  than the target then we need to move up in the btree to the
            //  parent to continue our search.
            //
            if ( diff >= 0 )
            {
                //
                //  If this is not the root node then move up in the btree.
                //
                if ( node->parent != 0 )
                {
                    BLOG ( "  Moving up to parent.\n" );
                    i    = 0;
                    node = cvtToAddr ( btree, node->parent );
                }
                //
                //  If this is the root node then the target record is larger
                //  than everything in the btree so return an "end" condition
                //  to the caller.
                //
                else
                {
                    BLOG ( "  End of tree found.\n" );
                    iter->index = -1;
                    iter->node  = 0;
                    iter->key   = 0;
                    goto nextExit;
                }
            }
            //
            //  There are some records in the right subtree greater than the
            //  target so go find the smallest of those records.
            //
            else
            {
                BLOG ( "  Searching for minimum subtree key.\n" );
                nodePos = get_min_key_pos ( btree,
                                            getLeftChild ( btree, node, i ) );
                //
                //  If the smallest record in this subtree is larger than our
                //  target key then we have found the "next" record and we
                //  will return that to the caller.
                //
                dataValue = getDataRecord ( btree, nodePos.node, nodePos.index );
                diff = btree_compare_function ( btree, key, dataValue );
                if ( diff < 0 )
                {
                    PRINT_DATA ( "    Found: ",
                             getDataRecord ( btree, nodePos.node, nodePos.index) );
                    iter->index = nodePos.index;
                    iter->node  = nodePos.node;
                    iter->key   = dataValue;
                    goto nextExit;
                }
                //
                //  If everything in the right subtree is smaller than our
                //  target, decend into the right tree and continue our search
                //  from there.
                //
                else
                {
                    BLOG ( "  Moving down to the right child.\n" );
                    node = getRightChild ( btree, node, index );
                    i    = 0;
                }
            }
        }
        continue;
    }
    //
    //  If we have found the "next" record, print that out in debug mode.
    //
    if ( iter->node != 0 )
    {
        BLOG ( "  Found index %d:\n", iter->index );
        PRINT_NODE ( btree, iter->node );
    }
    //
    //  Release the btree data structure lock.
    //
nextExit:

    BTREE_UNLOCK

    //
    //  Return to the caller now that we have updated the iterator to point
    //  to the "next" record in the btree.
    //
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ p r e v i o u s

    @brief Get the "previous" position in the btree.

    This function will position the specified iterator to the next lower key
    value in the btree from the current position.

    This function will use the position defined in the supplied iterator as
    the starting point in the btree and proceed to move to the "next lower"
    record in the btree.  Note that "next" is defined by the user's supplied
    comparison operator for this btree.

    @param[in,out] iter - The iterator to be updated

    @return None

-----------------------------------------------------------------------------*/
void btree_iter_previous ( btree_iter iter )
{
    BLOG ( "\nIn btree_iter_previous: node[%p], index[%d]\n", iter->node, iter->index );

    //
    //  Set the internal variables so that we will start searching where the
    //  current iterator points.
    //
    btree_t*     btree = iter->btree;
    bt_node_t*   node  = iter->node;
    void*        key   = iter->key;
    void*        dataValue;
    int          diff  = 0;
    int          i     = iter->index;
    nodePosition nodePos;

    //
    //  Print out the target key that we are starting at.
    //
    PRINT_DATA ( "  Looking up key:  ", key );

    //
    //  Obtain the data lock to make sure no one else is using the btree data
    //  structures.
    //
    BTREE_LOCK

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  If this is a leaf node...
        //
        if ( isLeaf ( node )  )
        {
            //
            //  Loop through the records in this node backwards until we reach
            //  the start of the node...
            //
            PRINT_DATA ( "  Current leaf node is: ",
                         getDataRecord ( btree, node, i ) );

            while ( i >= 0 )
            {
                //
                //  Compare the current record with our target.
                //
                diff = btree_compare_function ( btree, key,
                                                getDataRecord ( btree, node, i ) );

                BLOG ( "  Searching node at %d, diff: %d\n", i, diff );

                //
                //  If the target record is greater than the current record
                //  then we have found the "previous" record in the btree so
                //  return it to the caller.
                //
                if ( diff > 0 )
                {
                    BLOG ( "  Target record found.\n" );
                    iter->node  = node;
                    iter->index = i;
                    iter->key   = getDataRecord ( btree, node, i );
                    goto previousExit;
                }
                //
                //  Decrement to the next lower node index and continue
                //  looking.
                //
                --i;
            }
            //
            //  If none of the records in this node are smaller than the
            //  target, go up one level in the btree to the parent node and
            //  try again.
            //
            //  If we are at the root node then the target is smaller than the
            //  smallest record in the btree so return an "end" condition to
            //  the caller.
            //
            if ( node->parent == 0 )
            {
                BLOG ( "  Beginning of tree found.\n" );
                iter->node  = 0;
                iter->index = -1;
                iter->key   = 0;
                goto previousExit;
            }
            //
            //  Otherwise, we are not at the root node, so move up to the
            //  parent node and search that.
            //
            node = cvtToAddr ( btree, node->parent );
            i = node->keysInUse - 1;

            BLOG ( "  Moving up to the parent at: %p[%d]\n", node, i );

            continue;
        }
        //
        //  If the current node is an interior node (i.e. not a leaf node)...
        //
        PRINT_DATA ( "  Current interior node record is: ",
                     getDataRecord ( btree, node, i ) );
        //
        //  Examine all of the records until we reach the beginning of the
        //  records in this node.
        //
        while ( i >= 0 )
        {
            //
            //  Compare the current record with the target record.
            //
            dataValue = getDataRecord ( btree, node, i );
            diff = btree_compare_function ( btree, key, dataValue );

            BLOG ( "  Searching node at index %d, diff: %d\n", i, diff );

            //
            //  If the target is greater than the current record...
            //
            if ( diff > 0 )
            {
                //
                //  Go find the smallest record in the right subtree of this
                //  record.
                //
                nodePos = get_max_key_pos ( btree, getRightChild ( btree,
                                                                   node, i ) );
                dataValue = getDataRecord ( btree, nodePos.node, nodePos.index );

                BLOG ( "  Found the maximum key in the right child at %p[%d].\n",
                      nodePos.node, nodePos.index );

                diff = btree_compare_function ( btree, key, dataValue );
                if ( diff > 0 )
                {
                    //
                    //  Return this record to the caller.
                    //
                    PRINT_DATA ( "    Found: ", dataValue );

                    iter->node  = nodePos.node;
                    iter->index = nodePos.index;
                    iter->key   = dataValue;
                    goto previousExit;
                }
                else
                {
                    PRINT_DATA ( "  Target record found: ", dataValue );

                    iter->node  = node;
                    iter->index = i;
                    iter->key   = getDataRecord ( btree, node, i );
                    goto previousExit;
                }
            }
            //
            //  Decrement our node index value and check the previous value in
            //  this node.
            //
            --i;
        }
        //
        //  We have just exhausted the list of records in the current node...
        //
        //  We need to decide if we need to go up or down the btree from
        //  here so find the largest record in the left subtree.
        //
        i = 0;
        nodePos = get_max_key_pos ( btree, getLeftChild ( btree, node, i ) );
        dataValue = getDataRecord ( btree, nodePos.node, nodePos.index );
        diff = btree_compare_function ( btree, key, dataValue );

        //
        //  If there are no records in the left subtree that are smaller
        //  than the target then we need to move up in the btree to the
        //  parent to continue our search.
        //
        if ( diff <= 0 )
        {
            //
            //  If this is not the root node then move up in the btree.
            //
            if ( node->parent != 0 )
            {
                BLOG ( "  Moving up to parent.\n" );

                node = cvtToAddr ( btree, node->parent );
                i = node->keysInUse - 1;
            }
            //
            //  If this is the root node then the target record is smaller
            //  than everything in the btree so return an "iterator end"
            //  condition to the caller.
            //
            else
            {
                BLOG ( "  End of iterator found.\n" );
                iter->node  = 0;
                iter->index = -1;
                iter->key   = 0;
                goto previousExit;
            }
        }
        //
        //  There are some records in the left subtree smaller than the
        //  target so return the largest of these to the caller.
        //
        else
        {
            PRINT_DATA ( "    Found: ",
                         getDataRecord ( btree, nodePos.node, nodePos.index) );
            iter->node  = nodePos.node;
            iter->index = nodePos.index;
            iter->key   = dataValue;
            goto previousExit;
        }
        //
        //  Loop back up and continue our search with the node that we just
        //  moved into.
        //
        continue;
    }
    //
    //  If we have found the "previous" record, print that out in debug mode.
    //
    if ( iter->node != 0 )
    {
        BLOG ( "  Found node at index %d:\n", iter->index );
        PRINT_NODE ( btree, iter->node );
    }
    //
    //  Release the btree data structure lock.
    //
previousExit:

    BTREE_UNLOCK

    //
    //  Return to the caller now that we have updated the iterator to point
    //  to the "next" record in the btree.
    //
    return;
}




/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ c m p

    @brief Compare 2 iterators.

    This function will compare the data records using the user supplied
    comparison function of the two supplied iterators.

    @param[in] iter1 - The first position to be tested.
    @param[in] iter2 - The second position to be tested.

    @return < 0  - iter2 is greater than iter1.
            == 0 - The iterators are identical.
            > 0  - iter2 is less than iter1.

-----------------------------------------------------------------------------*/
int btree_iter_cmp ( btree_iter iter1, btree_iter iter2 )
{
    return btree_compare_function ( iter1->btree,
                    getDataRecord ( iter1->btree, iter1->node, iter1->index ),
                    getDataRecord ( iter2->btree, iter2->node, iter2->index ) );
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ a t _ e n d

    @brief Determine if this iterator is at the "end" condition.

    This function will determine if the input iterator is currently positioned
    at the "end" of the btree and return a "true" if it is.  Note that this
    can be used for the reverse iterator as well but in this case, the "end"
    condition is beyond the lowest value of the btree rather beyond the
    highest value of the btree.

    In this context, the term "at end" means "at the end of the iterator"
    rather than "at the end of the btree".

    @param[in] iter - The iterator to be tested.

    @return true  - The iterator is at the "end" of the btree.
            false - The iterator is not at the "end" of the btree.

-----------------------------------------------------------------------------*/
bool btree_iter_at_end ( btree_iter iter )
{
    return ( iter == NULL || iter->node == NULL );
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ d a t a

    @brief Return the user data pointer at the current position.

    This function will return the pointer to the user data structure that is
    stored in the btree at the current iterator position.  The user data
    structure address is the "value" portion of the key/value pairs that are
    stored in the btree.

    The iterator must have already been positioned somewhere in the btree
    using one of the positioning functions like btree_iter_find or a call to
    btree_iter_next, etc.

    @param[in] iter - The current btree iterator.

    @return The address of the user data at the current iterator position.
            NULL if the iterator has not been initialized.

-----------------------------------------------------------------------------*/
void* btree_iter_data ( btree_iter iter )
{
    //
    //  If this is not a valid iterator, return a NULL to the caller.
    //
    if ( btree_iter_at_end ( iter ) )
    {
        return NULL;
    }
    //
    //  Return the user data pointer to the caller.
    //
    return getDataRecord ( iter->btree, iter->node, iter->index );
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ c l e a n u p

    @brief Release all resources held by an iterator.

    This function will clean up the specified iterator and release any
    resources being used by this iterator.  This function MUST be called when
    the user is finished using an iterator or resources (such as memory
    blocks) will be leaked to the system.  Once this function has been called
    with an iterator, that iterator may not be used again for any of the btree
    functions unless it has subsequently been reinitialized using one of the
    iterator initialization functions defined here.

    Note: Iterators are allocated with the system malloc function so they get
    freed with "free" rather than the shared memory deallocator.

    @param[in] iter - The current btree iterator.

    @return None

-----------------------------------------------------------------------------*/
void btree_iter_cleanup ( btree_iter iter )
{
    //
    //  If this iterator is valid...
    //
    if ( iter != 0 )
    {
        //
        //  Set the iterator contents to "empty" just in case someone tries to
        //  use this iterator again after this call has freed it.
        //
        iter->btree = 0;
        iter->key   = 0;
        iter->node  = 0;
        iter->index = 0;

        //
        //  Go return this iterator data structure to the system memory pool.
        //
        free ( iter );
    }
    //
    //  Return to the caller.
    //
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ c o m p a r e _ f u n c t i o n

    @brief Compare 2 user defined data structures.

    This function will compare 2 user defined data structures by using the key
    definition data structure defined for the given btree.

    TODO: I've set an arbitrary maximum string length of 256 bytes so I could
    limit the searches using "strncmp".

    @param[in] btree - The address of the btree to be compared.
    @param[in] data1 - The address of the first user structure to compare.
    @param[in] data2 - The address of the second user structutre to compare.

    @return The result of comparing the two data structures.

-----------------------------------------------------------------------------*/
static int btree_compare_function ( btree_t* btree, void* data1, void* data2 )
{
    int              diff     = 0;
    offset_t         offset   = 0;
    btree_field_def* fieldDef = NULL;

    //
    //  Get the address of the key definition structures for this btree.
    //
    btree_key_def* keyDef = cvtToAddr ( btree, btree->keyDef );

    //
    //  For the number of fields to be compared for this btree...
    //
    for ( unsigned int i = 0; i < keyDef->fieldCount; ++i )
    {
        //
        //  Get the address of the field definition for the current field.
        //
        fieldDef = &keyDef->btreeFields[i];

        //
        //  Grab the offset into the data structure for the current field.
        //
        offset = fieldDef->offset;

        //
        //  Depending on what the field type is...
        //
        switch ( fieldDef->type )
        {
          //
          //    If the field type is invalid, generate an error message.
          //
          case ft_invalid:
              printf ( "Error: Invalid type[%d] found for field %u btree %p\n",
                       fieldDef->type, i, btree );
            break;

          //
          //    If the field type is "string", use the normal "strncmp"
          //    function to compare the 2 string pointers.
          //
          case ft_string:
            diff = strncmp ( cvtToAddr ( btree, *(offset_t*)(data1 + offset) ),
                             cvtToAddr ( btree, *(offset_t*)(data2 + offset) ),
                             256 );
                VERBOSE ( "Comparing String1: %s\n", (char*)cvtToAddr ( btree,
                                                   *(offset_t*)(data1 + offset)));
                VERBOSE ( "          String2: %s\n", (char*)cvtToAddr ( btree,
                                                   *(offset_t*)(data2 + offset)));
                VERBOSE ( "    Diff: %d\n", diff );
            break;

          //
          //    If the field type is a "char"...
          //
          case ft_char:
            diff = *(char*)( data1 + offset ) - *(char*)( data2 + offset );
            break;

          //
          //    If the field type is a "short"...
          //
          case ft_short:
            diff = *(short*)( data1 + offset ) - *(short*)( data2 + offset );
            break;

          //
          //    If the field type is a "int"...
          //
          case ft_int:
            diff = *(int*)( data1 + offset ) - *(int*)( data2 + offset );
            break;

          //
          //    If the field type is a "long"...
          //
          case ft_long:
            diff = *(long*)( data1 + offset ) - *(long*)( data2 + offset );
            break;

          //
          //    If the field type is a "long long"...
          //
          case ft_long_long:
            diff = *(long long*)( data1 + offset ) -
                   *(long long*)( data2 + offset );
            break;

          //
          //    If the field type is a "unsigned char"...
          //
          case ft_uchar:
            diff = *(unsigned char*)( data1 + offset ) -
                   *(unsigned char*)( data2 + offset );
            break;

          //
          //    If the field type is a "unsigned short"...
          //
          case ft_ushort:
            diff = *(unsigned short*)( data1 + offset ) -
                   *(unsigned short*)( data2 + offset );
            break;

          //
          //    If the field type is a "unsigned int"...
          //
          case ft_uint:
            diff = *(unsigned int*)( data1 + offset ) -
                   *(unsigned int*)( data2 + offset );
            break;

          //
          //    If the field type is a "unsigned long"...
          //
          case ft_ulong:
            diff = *(unsigned long*)( data1 + offset ) -
                   *(unsigned long*)( data2 + offset );
            break;

          //
          //    If the field type is a "unsigned long long"...
          //
          case ft_ulong_long:
            diff = *(unsigned long long*)( data1 + offset ) -
                   *(unsigned long long*)( data2 + offset );
            break;

          //
          //    If the field type is anything else, generate an error message.
          //
          default:
            printf ( "Error: Invalid type[%d] found for field %u btree %p\n",
                     fieldDef->type, i, btree );
            break;
        }
        //
        //  If the 2 fields just compared are not the same, we can quit
        //  comparing fields.
        //
        if ( diff != 0 )
        {
            break;
        }
    }
    //
    //  We have finished comparing the key fields so return the last
    //  difference value to the caller.
    //
    return diff;
}


//
//  Print the values of the key field on a single line.
//
static void btree_print_function ( btree_t* btree, void* data )
{
    offset_t         offset   = 0;
    btree_field_def* fieldDef = NULL;

    //
    //  Get the address of the key definition structures for this btree.
    //
    btree_key_def* keyDef = cvtToAddr ( btree, btree->keyDef );

    //
    //  For the number of fields to be compared for this btree...
    //
    for ( unsigned int i = 0; i < keyDef->fieldCount; ++i )
    {
        //
        //  Get the address of the field definition for the current field.
        //
        fieldDef = &keyDef->btreeFields[i];

        //
        //  Grab the offset into the data structure for the current field.
        //
        offset = fieldDef->offset;

        //
        //  If this is not the first key field, print a comman to separate the
        //  key field values.
        //
        if ( i != 0 )
        {
            printf ( ", " );
        }
        //
        //  Depending on what the field type is...
        //
        switch ( fieldDef->type )
        {
          //
          //    If the field type is invalid, generate an error message.
          //
          case ft_invalid:
              printf ( "Error: Invalid type[%d] found for field %u btree %p\n",
                       fieldDef->type, i, btree );
            break;

          //
          //    If the field type is "string", use the normal "strncmp"
          //    function to compare the 2 string pointers.
          //
          case ft_string:
            printf ( "%s", (char*)cvtToAddr ( btree, *(offset_t*)(data + offset) ) );
            break;

          //
          //    If the field type is a "char"...
          //
          case ft_char:
            printf ( "%c", *(char*)( data + offset ) );
            break;

          //
          //    If the field type is a "short"...
          //
          case ft_short:
            printf ( "%hd", *(short*)( data + offset ) );
            break;

          //
          //    If the field type is a "int"...
          //
          case ft_int:
            printf ( "%d", *(int*)( data + offset ) );
            break;

          //
          //    If the field type is a "long"...
          //
          case ft_long:
            printf ( "%ld", *(long*)( data + offset ) );
            break;

          //
          //    If the field type is a "long long"...
          //
          case ft_long_long:
            printf ( "%lld", *(long long*)( data + offset ) );
            break;

          //
          //    If the field type is a "unsigned char"...
          //
          case ft_uchar:
            printf ( "%c", *(unsigned char*)( data + offset ) );
            break;

          //
          //    If the field type is a "unsigned short"...
          //
          case ft_ushort:
            printf ( "%hd", *(unsigned short*)( data + offset ) );
            break;

          //
          //    If the field type is a "unsigned int"...
          //
          case ft_uint:
            printf ( "%u", *(unsigned int*)( data + offset ) );
            break;

          //
          //    If the field type is a "unsigned long"...
          //
          case ft_ulong:
            printf ( "%lu", *(unsigned long*)( data + offset ) );
            break;

          //
          //    If the field type is a "unsigned long long"...
          //
          case ft_ulong_long:
            printf ( "%llu", *(unsigned long long*)( data + offset ) );
            break;

          //
          //    If the field type is anything else, generate an error message.
          //
          default:
            printf ( "Error: Invalid type[%d] found for field %u btree %p\n",
                     fieldDef->type, i, btree );
            break;
        }
    }
    //
    //  Return to the caller.
    //
    return;
}


//
//  This is a helper function that will generate a relatively small integer
//  value from the difference between the current tree node address and the
//  base of the tree address.  This is being done to make it easier to
//  identify nodes in the dump routines that follow.  The integer values
//  generated don't have any meaning other than just a way to identify a node.
//
//  Note that the macro that follows is just a short way of specifying the
//  pair of "printf" arguments to print the actual node address followed by
//  this node identifier.
//
static offset_t blockId ( offset_t node )
{
    if ( node == 0 )
    {
        return 0;
    }
    return node / sizeof(struct bt_node_t);
}


//
//  Define the default function that will print the user data stored in the
//  B-tree.  This is defined as a "weak" function so that this implementation
//  will only be used if the user does not supply his own version of this
//  function.
//
//  Note that since this function makes explicit assumptions about the format
//  of the user data stored in the B-tree, it really should always be
//  implemented by the user.  Think of this function as a virtual function in
//  C++ that the user should redefine.
//
static void printFunction ( char* leader, void* data )
{
    //
    //  Define the "assumed" format of the incoming data structure.
    //
    struct userData
    {
        unsigned long domainId;
        unsigned long signalId;
        char*         name;
    };

    if ( data == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    return;
}


//
//  Define the default function that will be called with a pointer to user
//  data stored in the B-tree for each item stored in the B-tree.  This is
//  defined as a "weak" function so that this implementation will only be used
//  if the user does not supply his own version of this function.
//
//  Note that since this function makes explicit assumptions about the format
//  of the user data stored in the B-tree, it really should always be
//  implemented by the user.  Think of this function as a virtual function in
//  C++ that the user should redefine.
//
void traverseFunction ( void* data )
{
    //
    //  This default implementation merely calls the print function to print
    //  the data that is passed in.
    //
    printFunction ( "", data );

    return;
}


//
//  Print the information about a single btree node.
//
static void print_single_node ( btree_t* btree, bt_node_t* node,
                                printFunc printCB )
{
    offset_t offset = cvtToOffset ( btree, node );

    printf ( "\n  Node[%p:%lx:%ld]\n",     node, offset, blockId(offset) );

    offset = node->next;
    printf ( "    next.......: %lx:%ld\n", offset, blockId(offset) );

    offset = node->parent;
    printf ( "    parent.....: %lx:%ld\n", offset, blockId(offset) );
    printf ( "    leaf.......: %s\n",      isLeaf ( node ) ? "Yes" : "No" );
    printf ( "    keysInUse..: %d\n",      node->keysInUse );
    printf ( "    level......: %d\n",      node->level );

    offset = node->dataRecords;
    printf ( "    dataRecords: %lx:%ld\n", offset, blockId(offset) );

    offset = node->children;
    printf ( "    children...: %lx:%ld\n", offset, blockId(offset) );

    // DUMP_TL ( node, sizeof(bt_node_t), "\n   ", 4 );

    void*    lChild;
    void*    rChild;
    offset_t lChildOffset;
    offset_t rChildOffset;

    //
    //  For each record in this node...
    //
    for ( int i = 0; i < node->keysInUse; ++i )
    {
        //
        //  If this is an interior node, print the left and right children of
        //  each record.
        //
        if ( ! isLeaf ( node ) )
        {
            lChild = getLeftChild  ( btree, node, i );
            rChild = getRightChild ( btree, node, i );

            lChildOffset = cvtToOffset ( btree, lChild );
            rChildOffset = cvtToOffset ( btree, rChild );

            printf ( "      index: %d, left[%lx:%ld], right[%lx:%ld], ", i,
                     lChildOffset, blockId ( lChildOffset ),
                     rChildOffset, blockId ( rChildOffset ) );
        }
        //
        //  If this is a leaf node, just print some leading blanks that will
        //  preceed the key/value pairs that will be printed on this line
        //  later.
        //
        else
        {
            printf ( "      " );
        }
        //
        //  If the caller supplied a callback function to print the keys and
        //  values, call his function.
        //
        if ( printCB != NULL )
        {
            printCB ( "", getDataRecord ( btree, node, i ) );
        }
        //
        //  If the caller did not supply a callback function, use the builtin
        //  function that just prints the raw key/value data.
        //
        else
        {
            //
            //  Print the output leader string.
            //
            printf ( "index: %d, key: ", i );

            //
            //  Go output the key values at this index location.
            //
            btree_print_function ( btree, getDataRecord ( btree, node, i ) );
        }
        //
        //  Print out the line terminator for this key.
        //
        printf ( "\n" );
    }
    fflush ( stdout );
}


static void btree_print_header ( btree_t* btree )
{
    if ( btree == 0 )
    {
        printf ( "Error: Attempt to print NULL btree!\n" );
        exit ( 255 );
    }
    BLOG ( "\nB-tree parameters set to the following:\n" );
    BLOG ( "    B-tree location: %p\n",  btree );
    BLOG ( "          Node Size: %u\n",  btree->nodeSize );
    BLOG ( "          Tree Type: %s\n",  btree->type == TYPE_USER ?
                                       "user" : "system" );
    BLOG ( "      Max Recs/Node: %u\n",  btree->maxRecCnt );
    BLOG ( "         Min Degree: %u\n",  btree->minDegree );
    BLOG ( "        Min Records: %u\n",  btree->min );
    BLOG ( "      Current Count: %u\n",  btree->count );
    BLOG ( "          Root Node: 0x%lx\n", btree->root );

    btree_key_def* keyDefinition = cvtToAddr ( btree, btree->keyDef );

    BLOG ( "  Num of Key Fields: %u\n", keyDefinition->fieldCount );
    for ( int i = 0; i < keyDefinition->fieldCount; ++i )
    {
        BLOG ( "            Field %d: type[%d], offset[%d], size[%d]\n",
               i,  keyDefinition->btreeFields[i].type,
               keyDefinition->btreeFields[i].offset,
               keyDefinition->btreeFields[i].size );
    }
    DUMP_TL ( btree, sizeof(btree_t), "\n ", 2 );

    return;
}


/**
*       Function used to print the B-tree
*       @param root Root of the B-Tree
*       @param print_key Function used to print the key value
*       @return none
*/

void print_subtree ( btree_t* btree, bt_node_t* node, printFunc printCB )
{
    int i = 0;
    unsigned int current_level;

    bt_node_t* head;
    bt_node_t* tail;
    bt_node_t* child;

    current_level = node->level;
    head = node;
    tail = node;

    offset_t headOffset = cvtToOffset ( btree, head );

    btree_print_header ( btree );

    //
    //  Traverse the nodes of the btree displaying them as we go.
    //
    while ( headOffset != 0 )
    {
        if ( head->level < current_level )
        {
            current_level = head->level;
        }
        print_single_node ( btree, head, printCB );

        if ( ! isLeaf ( head ) )
        {
            for ( i = 0; i < head->keysInUse + 1; i++ )
            {
                child = getLeftChild ( btree, head, i );
                tail->next = cvtToOffset ( btree, child );
                tail = child;
                child->next = 0;
            }
        }
        headOffset = head->next;
        head = cvtToAddr ( btree, headOffset );
    }
    BLOG ( "\n" );
}


//
//  Define the user space call to print an entire btree.
//
void btree_print ( btree_t* btree, printFunc printCB )
{
    if ( btree->type == TYPE_USER )
    {
        print_subtree ( btree, cvtToAddr ( btree, btree->root ), printCB );
    }
}


#ifdef BTREE_VALIDATE
/*!----------------------------------------------------------------------------

    The following functions are here as an example of a set of "validation"
    functions that have been used to validate the structure of a btree.  These
    are here merely as an example.

    Note that all of these functions rely on the fact that the "domainId"
    field of the user data record is an integer value that corresponds to the
    "record number" in the btree.  In other words, when the 4th user record is
    created, the domainId is set to 3 (since the counters start at zero).

-----------------------------------------------------------------------------*/
//
//  Validation functions.
//
static int  validateCount = 0;
static int* validateIds   = 0;

//
//  The following is the user defined data structure that all of the records
//  in the btree will point to.
//
typedef struct userData
{
    unsigned long domainId;
    unsigned long signalId;
    char*         name;

}   userData;


static void validateReset ( void )
{
    for ( int i = 0; i < validateCount; ++i )
    {
        validateIds[i] = 0;
    }
}


static void validateInit ( int maxSize )
{
    validateCount = maxSize;
    validateIds   = malloc ( maxSize * sizeof(int) );

    validateReset();
}


static void validateRecord ( char* leader, void* recordPtr )
{
    userData*     data = recordPtr;
    unsigned long item = data->domainId;

    validateIds[item]++;
}


static int validateReport ( bool reportMissing, int firstValid, int lastValid )
{
    int errorCount = 0;

    for ( int i = 0; i < validateCount; ++i )
    {
        if ( reportMissing )
        {
            if ( validateIds[i] == 0 && i >= firstValid && i <= lastValid &&
                 i != 11 )
            {
                printf ( "Error: validation found missing record %d.\n", i );
                errorCount++;
            }
        }
        else
        {
            if ( validateIds[i] != 0 && i >= firstValid && i <= lastValid &&
                 i != 11 )
            {
                printf ( "Error: validation found not missing record %d.\n", i );
                errorCount++;
            }
        }
        if ( validateIds[i] > 1 )
        {
            printf ( "Error: validation found record %d duplicated %d "
                     "time(s).\n", i, validateIds[i] - 1 );
            errorCount++;
        }
    }
    return errorCount;
}


static void validateCleanup ( void )
{
    validateCount = 0;
    free ( validateIds );
    validateIds = 0;
}


void validateBtree ( btree_t* btree, int maxSize, bool reportMissing,
                     int firstValid, int lastValid )
{
    int errorCount = 0;

    printf ( "Validating tree: %p, size: %d, reportMissing: %d from %d to %d\n",
             btree, maxSize, reportMissing, firstValid, lastValid );

    if ( validateCount == 0 )
    {
        validateInit ( maxSize );
    }
    else
    {
        validateReset();
    }
    btree_traverse ( btree, validateRecord );

    errorCount = validateReport ( reportMissing, firstValid, lastValid );

    printf ( "Validation found %d error(s).\n", errorCount );

    validateCleanup();
}


//
//  Define a function that we'll use to test the btree_find function for
//  creating an iterator.
//
void findTest ( btree_t* btree, int domain, int signal, bool validate )
{
    userData   scratch = { 0 };
    userData*  tempData = 0;
    btree_iter iter;

    scratch.domainId = domain;
    scratch.signalId = signal;

    iter = btree_find ( btree, &scratch );

    if ( ! btree_iter_at_end ( iter ) )
    {
        tempData = btree_iter_data ( iter );

        printf ( "Iterator position for %lu,%lu found %lu,%lu in %p:%d\n",
                 scratch.domainId, scratch.signalId,
                 tempData->domainId, tempData->signalId,
                 iter->node, iter->index );

        if ( validate && tempData->domainId != scratch.domainId )
        {
            printf ( "Error: Expected %lu,%lu, found %lu,%lu\n",
                     scratch.domainId, scratch.signalId,
                     tempData->domainId, tempData->signalId );
        }
        btree_iter_cleanup ( iter );
    }
    else
    {
        printf ( "Iterator position for %lu,%lu is end()\n",
                 scratch.domainId, scratch.signalId );
    }
}

//
//  Define a function that we'll use to test the btree_rfind function for
//  creating an iterator.
//
void rFindTest ( btree_t* btree, int domain, int signal, bool validate )
{
    userData   scratch = { 0 };
    userData*  tempData = 0;
    btree_iter iter;

    scratch.domainId = domain;
    scratch.signalId = signal;

    iter = btree_rfind ( btree, &scratch );

    if ( ! btree_iter_at_end ( iter ) )
    {
        tempData = btree_iter_data ( iter );

        printf ( "Iterator position for %lu,%lu found %lu,%lu in %p:%d\n",
                 scratch.domainId, scratch.signalId,
                 tempData->domainId, tempData->signalId,
                 iter->node, iter->index );

        if ( validate && tempData->domainId != scratch.domainId )
        {
            printf ( "Error: Expected %lu,%lu, found %lu,%lu\n",
                     scratch.domainId, scratch.signalId,
                     tempData->domainId, tempData->signalId );
        }
        btree_iter_cleanup ( iter );
    }
    else
    {
        printf ( "Iterator position for %lu,%lu is end()\n",
                 scratch.domainId, scratch.signalId );
    }
}


#endif  // #ifdef BTREE_VALIDATE

// vim:filetype=c:syntax=c
