#ifndef _BTREE_H_
#define _BTREE_H_

#define DEBUG

// Platform dependent headers
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <stddef.h>
#include <stdbool.h>

#define MEM_ALLOC malloc
#define MEM_FREE  free
#define BCOPY     bcopy
#define PRINT     printf
#define BTREE_CMP btree->value

//
//  Define the btree record structure.  In this incarnation of the btree
//  algorithm, the keys are pointers to a user specified data structure.  The
//  keys and values are then contained in that user data structure and the
//  btree supplied comparison function is called to compare the keys in two
//  instances of these records.
//
typedef int (*compareFunc)( void*, void* );

typedef void (*printFunc)( char*, void* );

typedef void (*traverseFunc)( void* );


typedef struct bt_node
{
    struct bt_node*  next;        // Pointer used for linked list
    bool             leaf;        // Used to indicate whether leaf or not
    unsigned int     keysInUse;   // Number of active keys
    unsigned int     level;       // Level in the btree
    void**           dataRecords; // Array of keys and values
    struct bt_node** children;    // Array of pointers to child nodes

}   bt_node;

typedef struct btree
{
    unsigned int order;           // B-Tree order
    unsigned int nodeFullSize;    // The number of records in a full node
    unsigned int sizeofKeys;      // The total size of the keys in one node
    unsigned int sizeofPointers;  // The total size of the pointers in one node
    unsigned int count;           // The total number of records in the btree
    bt_node*     root;            // Root of the btree
    compareFunc  compareCB;       // Key compare function
    printFunc    printCB;         // Print the contents of the current record

}   btree;

extern btree* btree_create ( unsigned int order, compareFunc compareFunction,
                             printFunc printFunction );

extern int btree_insert ( btree* btree, void* data );

extern int btree_delete ( btree* btree, bt_node* subtree, void* key );

extern void* btree_get_min ( btree* btree );

extern void* btree_get_max ( btree* btree );

extern void* btree_search ( btree* btree, void* key );

extern void btree_traverse ( btree* btree, traverseFunc traverseFunction );

extern void btree_destroy ( btree* btree );

// extern unsigned int btree_get_count ( btree* btree );

static inline unsigned int btree_get_count ( btree* btree )
{
    return btree->count;
};


#ifdef DEBUG
extern void print_subtree ( btree* btree, bt_node* node );
#endif


#endif
