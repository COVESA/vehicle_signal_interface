/*
    Copyright (C) 2016-2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

    @file      btreeTests.c

    This file contains some fairly extensive tests of the shared memory btree
    code.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>


#include "vsi.h"
#include "vsi_core_api.h"
#include "signals.h"

#undef TEST_BTREE_NAMES_INDEX

#define BTREE_VALIDATE

//
//  Conditionally define the calls to "findTest".
//
#ifdef BTREE_VALIDATE
#   define PRINT_TREE btree_print
#else
#   define PRINT_TREE(...)
#endif

/*! @{ */


//
//  The following is the user defined data structure that all of the records
//  in the btree will point to.
//
#define MAX_NAME_LEN ( 32 )

typedef struct userData
{
    unsigned long domainId;
    unsigned long signalId;
    char          name[MAX_NAME_LEN + 1];

}   userData;


//
//  This function will compare 2 pointers to the user defined data structures
//  above.  This function will do the comparison based on the domain and
//  signal ID.
//
int compareIds ( void* a, void* b )
{
    userData* userDataPtr1 = a;
    userData* userDataPtr2 = b;
    int       result;

    printf ( "Comparing %p: %lu, %lu with %p: %lu, %lu\n",
          userDataPtr1, userDataPtr1->domainId, userDataPtr1->signalId,
          userDataPtr2, userDataPtr2->domainId, userDataPtr2->signalId );

    if ( ( result = userDataPtr1->domainId - userDataPtr2->domainId ) == 0 )
    {
        return userDataPtr1->signalId - userDataPtr2->signalId;
    }
    return result;
}


//
//  This function will compare 2 pointers to the user defined data structures
//  above.  This function will do the comparison based on the signal name.
//
int compareNames ( void* a, void* b )
{
    userData* userDataPtr1 = a;
    userData* userDataPtr2 = b;

    printf ( "Comparing [%s] with [%s]\n", userDataPtr1->name,
             userDataPtr2->name );

    return strcmp ( userDataPtr1->name, userDataPtr2->name );
}


//
//  This function will be called by the btree code when it has been called to
//  print out the btree nodes.  Each userData entry that is encountered in
//  that process will result in a call to this function to render the data the
//  way the user wants it to be displayed.
//
void printFunction ( char* leader, void* data )
{
    // LOG ( "printFunction called with leader: %p, data: %p\n", leader, data );

    if ( data == NULL )
    {
        printf ( "%s(nil)\n", leader );
        return;
    }
    userData* userDataPtr = data;

    printf ( "%sdomainId: %lu, signalId: %lu, name[%s]", leader,
             userDataPtr->domainId, userDataPtr->signalId, userDataPtr->name );
}


//
//  This function will be called by the btree code when it has been called to
//  traverse the tree in userData order.  Each userData entry that is
//  encountered in the traversal process will result in a call to this
//  function with the address of the user data structure as it's argument.
//
//  This specific case is going to want to print out the data in the user data
//  record so it calls the print function to do that each time it is called
//  with a new btree record.
//
static unsigned long traverseCount = 0;

void traverseFunction ( char* leader, void* dataPtr )
{
    if ( dataPtr == NULL )
    {
        printf ( "(nil)\n" );
        return;
    }
    printFunction ( "  ", dataPtr );
    printf ( "\n" );

    userData* userDataPtr = dataPtr;
    if ( userDataPtr->domainId != traverseCount )
    {
        if ( traverseCount != 11 )
        {
            printf ( "Error: Tree traversal found record %lu - Should be %lu\n",
                     userDataPtr->domainId, traverseCount );
        }
        else
        {
            traverseCount = userDataPtr->domainId;
        }
    }
    ++traverseCount;
}


//
//  Define the usage message function.
//
static void usage ( const char* executable )
{
    printf ( " \n\
Usage: %s options\n\
\n\
  Option     Meaning       Type     Default   \n\
  ======  ==============  ======  =========== \n\
    -c    Record Count     int        200     \n\
    -o    Btree Order      int         2      \n\
    -h    Help Message     N/A        N/A     \n\
    -?    Help Message     N/A        N/A     \n\
\n\n\
",
             executable );
}


//
//    m a i n
//
int main ( int argc, char *argv[] )
{
    int       recordCount = 200;
    int       order = 2;
    int       i = 0;
    int       status = 0;
    userData* data;
    userData* tempData;
    userData  scratch = { 0 };

#ifdef TEST_BTREE_NAMES_INDEX
    btree_t  nameTree;
#endif

    //
    //  The following locale settings will allow the use of the comma
    //  "thousands" separator format specifier to be used.  e.g. "10000"
    //  will print as "10,000" (using the %'u spec.).
    //
    setlocale ( LC_ALL, "");

    //
    //  Parse any command line options the user may have supplied.
    //
    char ch;

    while ( ( ch = getopt ( argc, argv, "c:o:h?" ) ) != -1 )
    {
        switch ( ch )
        {
          //
          //    Get the record count option flag if present.  This is the
          //    maximum number of records that will be inserted into the
          //    trees.
          //
          case 'c':
            recordCount = atol ( optarg );
            printf ( "The maximum record count has been set to %d\n", recordCount );
            break;

          //
          //    Get the btree order value if present.
          //
          case 'o':
            order = atol ( optarg );
            printf ( "The Btree order value has been set to %d\n", order );
            break;

          //
          //    Display the help message.
          //
          case 'h':
          case '?':
          default:
            usage ( argv[0] );
            exit ( 0 );
        }
    }
    //
    //  If the user supplied any arguments not parsed above, they are not
    //  valid arguments so complain and quit.
    //
    argc -= optind;
    if ( argc != 0 )
    {
        printf ( "Invalid parameters[s] encountered: %s\n", argv[argc] );
        usage ( argv[0] );
        exit (255);
    }
    //
    //  Open the shared memory file.
    //
    //  Note that if the shared memory segment does not already exist, this
    //  call will create it.
    //
    // TODO: Change to "false" for running!
    vsi_initialize ( true );

    //
    //  Create 2 btree indices on the same set of user data structures.  The 2
    //  indices are defined by the comparison callback function that is
    //  supplied in these calls.
    //
    btree_key_def* idKeyDef = sm_malloc ( KEY_DEF_SIZE(2) );
	idKeyDef->fieldCount = 2;
	idKeyDef->btreeFields[0].type   = ft_ulong;
    idKeyDef->btreeFields[0].offset = offsetof ( userData, domainId );
    idKeyDef->btreeFields[0].size   = 1;

    idKeyDef->btreeFields[1].type   = ft_ulong;
    idKeyDef->btreeFields[1].offset = offsetof ( userData, signalId );
    idKeyDef->btreeFields[1].size   = 1;

    btree_t* idTree = btree_create ( 21, idKeyDef );

    printf ( "Created ID B-tree:\n" );
    btree_print ( idTree, 0 );

#ifdef TEST_BTREE_NAMES_INDEX
    btree_key_def* offsetKeyDef = sm_malloc ( KEY_DEF_SIZE(1) );
	offsetKeyDef->fieldCount = 1;
	offsetKeyDef->btreeFields[0].type   = ft_string;
    offsetKeyDef->btreeFields[0].offset = offsetof ( userData, name );
    offsetKeyDef->btreeFields[0].size   = 1;

    btree_initialize ( nameTree, 15, offsetKeyDef );

    printf ( "Created ID B-tree:\n" );
    btree_print ( nameTree, 0 );

#endif

    //-----------------------------------------------------------------------
    //
    //  Insert one record.
    //
    printf ( "\nTEST 1\n" );
    printf ( "\nInsert/delete one record...\n" );

    scratch.domainId = 11;
    scratch.signalId = 11 * 11;

    printf ( "  Inserting record %d\n", 11 );
    btree_insert ( idTree, &scratch );
    PRINT_TREE ( idTree, printFunction );
    if ( idTree->count != 1 )
    {
        printf ( "Error: Insert of record 11 failed!\n" );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
        exit (255);
    }

    VALIDATE_BTREE ( idTree, 1, false, 11, 11 );

    //-----------------------------------------------------------------------
    //
    //  Delete one record.
    //
    printf ( "\nTEST 2\n\n" );
    printf ( "  Deleting record %d\n", 11 );
    status = btree_delete ( idTree, &scratch );
    printf ( "  Delete resulted in: %d\n", status );
    PRINT_TREE ( idTree, printFunction );
    if ( idTree->count != 0 )
    {
        printf ( "Error: Insert of record 11 failed!\n" );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
        exit (255);
    }

    VALIDATE_BTREE ( idTree, 1, false, 0, 0 );

    //-----------------------------------------------------------------------
    //
    //  Create a number of new userData instances and insert them into both of
    //  the btrees...
    //
    printf ( "\nTEST 3\n" );
    printf ( "\nCreate a bunch of records...\n" );

    for ( i = 0; i < recordCount; ++i )
    {
        //
        //  Go allocate a new userData data structure.
        //
        data = sm_malloc ( sizeof(userData) );

        //
        //  Initialize the fields in the data structure.
        //
        data->domainId = i;
        data->signalId = i * 11;
        sprintf ( data->name, "%lu-%lu", data->domainId, data->signalId );
        data->name[MAX_NAME_LEN] = 0;

        //
        //  Go insert this new data structure into both of the btrees that we
        //  created.
        //
        btree_insert ( idTree, data );
#ifdef TEST_BTREE_NAMES_INDEX
        btree_insert ( nameTree, data );
#endif

        printf ( "After inserting record %d\n", i + 1 );
        PRINT_TREE ( idTree, printFunction );
        // validatePointers ( idTree->root );
        traverseCount = 0;
        btree_traverse ( idTree, traverseFunction );
    }
    printf ( "When all done inserting...\n" );
    PRINT_TREE ( idTree, printFunction );

#ifdef TEST_BTREE_NAMES_INDEX
    PRINT_TREE ( nameTree, printFunction );
#endif

    if ( idTree->count != recordCount )
    {
        printf ( "Error: Insert of %d records failed!\n", recordCount );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
        exit (255);
    }
    //-----------------------------------------------------------------------
    //
    //  Perform a tree traversal of the ID btree to verify that the sorting
    //  worked properly.
    //
    //  Note that the "traversefunction" will be called by the btree_traverse
    //  function with a pointer to a "userData" stucture each time the
    //  traversal function finds a new record in the btree.
    //
    printf ( "TEST 4\n\n" );
    VALIDATE_BTREE ( idTree, recordCount, true, 0, recordCount - 1 );
    printf ( "\nTraverse all the data records in order...\n" );
    traverseCount = 0;
    btree_traverse ( idTree, traverseFunction );

    //-----------------------------------------------------------------------
    //
    //  Test some of the combinations of minimum and maximum values for both
    //  of the btrees.  Notice that the "name" tree is sorted differently from
    //  the ID tree due to the way the comparison function works.  e.g. Notice
    //  that "9-99" in the name tree is greater than "19-209", whereas the
    //  ordering of these two items is reversed in the ID tree.
    //
    printf ( "\nTEST 5\n" );
    printf ( "\nPrint the minimum value of the data...\n" );
    tempData = btree_get_min ( idTree );
    printf ( "  Minimum value by ID:   " );
    printFunction ( "", tempData );
    printf ( "\n" );
    if ( tempData->domainId != 0 )
    {
        printf ( "Error: Minimum value fetch failed.  Found %lu, should be %d\n",
                 tempData->domainId, 0 );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
        exit (255);
    }

#ifdef TEST_BTREE_NAMES_INDEX
    tempData = btree_get_min ( nameTree );
    printf ( "  Minimum value by Name: " );
    printFunction ( "", tempData );
    printf ( "\n" );
#endif

    //-----------------------------------------------------------------------
    printf ( "\nTEST 6\n" );
    printf ( "\nPrint the maximum value of the data...\n" );
    tempData = btree_get_max ( idTree );
    printf ( "  Maximum value by ID:   " );
    printFunction ( "", tempData );
    printf ( "\n" );
    if ( tempData->domainId != recordCount - 1 )
    {
        printf ( "Error: Maximum value fetch failed.  Found %lu, should be %d\n",
                 tempData->domainId, recordCount - 1 );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
        // exit (255);
    }

#ifdef TEST_BTREE_NAMES_INDEX
    tempData = btree_get_max ( nameTree );
    printf ( "  Maximum value by Name: " );
    printFunction ( "", tempData );
    printf ( "\n" );
#endif

    //-----------------------------------------------------------------------
    //
    //  Make sure that the search functions work and fail appropriately.
    //
    printf ( "\nTEST 7\n" );
    printf ( "\nSequential search - one at a time...\n" );
    for ( i = 0; i < recordCount; ++i )
    {
        scratch.domainId = i;
        scratch.signalId = i * 11;
        tempData = btree_search ( idTree, &scratch );
        if ( tempData == NULL )
        {
            printf ( "Error: Search value failed.  No record for %d, %d found.\n",
                     i, i*11 );
#ifdef BTREE_DEBUG
            btree_print ( idTree, 0 );
#endif
            exit (255);
        }
        if ( tempData->domainId != i )
        {
            printf ( "Error: Search value failed.  Found %lu, should be %d\n",
                     tempData->domainId, i );
#ifdef BTREE_DEBUG
            btree_print ( idTree, 0 );
#endif
            exit (255);
        }
        printf ( "  Search for %d resulted in: ", i );
        printFunction ( "", tempData );
        printf ( "\n" );
    }
    //-----------------------------------------------------------------------
    printf ( "\nTEST 8\n" );
    printf ( "\nPerform an unsuccessful search...\n" );
    // validateBtree ( idTree, 20, true, 0, 209 );
    scratch.domainId = 2;
    scratch.signalId = 33;
    tempData = btree_search ( idTree, &scratch );
    printf ( "  Search for 2, 33 [should fail!] resulted in: " );
    printFunction ( "", tempData );
    printf ( "\n" );
    if ( tempData != 0 )
    {
        printf ( "Error: Search value should have failed.  Found %lu, should be %d\n",
                 tempData->domainId, 0 );
        // validateBtree ( idTree, 20, true, 0, 209 );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
        exit (255);
    }
    //-----------------------------------------------------------------------
    //
    //  Test the iterator logic.
    //
    //  Perform the iterator find logic test.
    //
    printf ( "TEST 9\n" );
    printf ( "\nPerform btree iterator find tests...\n" );

    PRINT_TREE ( idTree, printFunction );

    btree_iter iter = 0;

    for ( i = 0; i < recordCount; ++i )
    {
        FIND_TEST ( idTree, i, i * 11, true );
    }
    printf ( "\n" );
    FIND_TEST ( idTree, 15, 0, false );
    FIND_TEST ( idTree, 15, 222, false );
    FIND_TEST ( idTree, 5, 10, false );
    FIND_TEST ( idTree, 5, 222, false );
    FIND_TEST ( idTree, 7, 20, false );
    FIND_TEST ( idTree, 7, 222, false );
    FIND_TEST ( idTree, 18, 2, false );
    FIND_TEST ( idTree, 18, 222, false );
    FIND_TEST ( idTree, recordCount - 1, 9999999, false );
    FIND_TEST ( idTree, recordCount - 1, 0, false );

    //-----------------------------------------------------------------------
    printf ( "\nTEST 10\n" );
    printf ( "\nPerform btree iterator next tests...\n" );

    iter = btree_iter_begin ( idTree );

    i = 0;
    while ( ! btree_iter_at_end ( iter ) )
    {
        tempData = btree_iter_data ( iter );

        if ( tempData->domainId != i++ )
        {
            printf ( "Error: Expected %d,%d, found %lu,%lu\n",
                     i, i * 11,
                     tempData->domainId, tempData->signalId );
#ifdef BTREE_DEBUG
            btree_print ( idTree, 0 );
#endif
        }
        else
        {
            printf ( "Iterator next value found %lu,%lu in %p:%d\n",
                     tempData->domainId, tempData->signalId,
                     iter->node, iter->index );
        }
        btree_iter_next ( iter );
    }
    btree_iter_cleanup ( iter );

    //-----------------------------------------------------------------------
    //
    //  Perform the reverse iterator find logic test.
    //
    printf ( "\nTEST 11\n" );
    printf ( "\nPerform btree reverse iterator find tests...\n" );

    PRINT_TREE ( idTree, printFunction );

    iter = 0;

    for ( i = 0; i < recordCount; ++i )
    {
        RFIND_TEST ( idTree, i, i * 11, true );
    }
    printf ( "\n" );
    RFIND_TEST ( idTree, 15, 0, false );
    RFIND_TEST ( idTree, 15, 222, false );
    RFIND_TEST ( idTree, 5, 10, false );
    RFIND_TEST ( idTree, 5, 222, false );
    RFIND_TEST ( idTree, 7, 20, false );
    RFIND_TEST ( idTree, 7, 222, false );
    RFIND_TEST ( idTree, 18, 2, false );
    RFIND_TEST ( idTree, 18, 222, false );
    RFIND_TEST ( idTree, recordCount - 1, 999999, false );
    RFIND_TEST ( idTree, recordCount - 1, 0, false );

    //-----------------------------------------------------------------------
    printf ( "\nTEST 12\n" );
    printf ( "\nPerform btree reverse iterator next tests...\n" );

    iter = btree_iter_end ( idTree );

    i = recordCount;
    while ( ! btree_iter_at_end ( iter ) )
    {
        --i;
        tempData = btree_iter_data ( iter );

        if ( tempData->domainId != i )
        {
            printf ( "Error: Expected %d,%d, found %lu,%lu\n",
                     i, i * 11,
                     tempData->domainId, tempData->signalId );
#ifdef BTREE_DEBUG
            btree_print ( idTree, 0 );
#endif
        }
        else
        {
            printf ( "Iterator previous value found %lu,%lu in %p:%d\n",
                     tempData->domainId, tempData->signalId,
                     iter->node, iter->index );
        }
        btree_iter_previous ( iter );
    }
    btree_iter_cleanup ( iter );

    //-----------------------------------------------------------------------
    //
    //  Test the iterator forward/reverse functions.
    //
    printf ( "\nTEST 13\n" );
    printf ( "\nForward/Reverse iterator test...\n" );

    scratch.domainId = 7;
    scratch.signalId = 7;
    printf ( "btree_find is looking for record 7,7...\n " );

    iter = btree_find ( idTree, &scratch );
    if ( iter == NULL )
    {
        printf ( "Error: Unable to allocate memory for an iterator!\n"
                 "  or unable to find the specified record\n" );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
    }
    printf ( "btree_find found " );
    printFunction ( "", iter->key );
    printf ( "\n" );

    btree_iter_next ( iter );
    printf ( "btree_iter_next found " );
    printFunction ( "", iter->key );
    printf ( "\n" );

    btree_iter_previous ( iter );
    printf ( "btree_iter_previous found " );
    printFunction ( "", iter->key );
    printf ( "\n" );

    btree_iter_cleanup ( iter );

    scratch.domainId = 7;
    scratch.signalId = 80;
    printf ( "\nbtree_rfind is looking for record 7,80...\n " );

    iter = btree_rfind ( idTree, &scratch );
    if ( iter == NULL )
    {
        printf ( "Error: Unable to allocate memory for an iterator!\n"
                 "  or unable to find the specified record\n" );
#ifdef BTREE_DEBUG
        btree_print ( idTree, 0 );
#endif
    }
    printf ( "btree_rfind found " );
    printFunction ( "", iter->key );
    printf ( "\n" );

    btree_iter_next ( iter );
    printf ( "btree_iter_next found " );
    printFunction ( "", iter->key );
    printf ( "\n" );

    btree_iter_previous ( iter );
    printf ( "btree_iter_previous found " );
    printFunction ( "", iter->key );
    printf ( "\n" );

    btree_iter_cleanup ( iter );

    //-----------------------------------------------------------------------
    //
    //  Delete record 11 (because it was a known problem).
    //
    printf ( "\nTEST 14\n" );
    printf ( "\nDelete record 11...\n" );

    i = 11;
    scratch.domainId = i;
    scratch.signalId = i * 11;
    printf ( "  Deleting record %d\n", i );
    status = btree_delete ( idTree, &scratch );
    printf ( "  Delete resulted in: %d\n", status );

    printf ( "After deleting record %d\n", i );
    PRINT_TREE ( idTree, printFunction );

    printf ( "Traverse all the data records in order...\n" );
    traverseCount = 0;
    btree_traverse ( idTree, traverseFunction );
    VALIDATE_BTREE ( idTree, recordCount, false, 11, 11 );

    //-----------------------------------------------------------------------
    //
    //  Delete all the records in the ID tree.
    //
    printf ( "\nTEST 15\n\n" );
    for ( i = 0; i < recordCount; ++i )
    {
        scratch.domainId = i;
        scratch.signalId = i * 11;
        printf ( "  Deleting record %d\n", i );
        status = btree_delete ( idTree, &scratch );
        printf ( "  Delete resulted in: %d\n", status );

        printf ( "After deleting record %d\n", i );
        PRINT_TREE ( idTree, printFunction );
        VALIDATE_BTREE ( idTree, recordCount, false, 0, i );
    }
    //-----------------------------------------------------------------------
    printf ( "\nTEST 16\n" );
    printf ( "\nTraverse all the data records in order...\n" );
    traverseCount = 0;
    btree_traverse ( idTree, traverseFunction );

    //-----------------------------------------------------------------------
    printf ( "\nTEST 17\n" );
    printf ( "\nFinal validation of empty tree.\n" );
    PRINT_TREE ( idTree, printFunction );
    VALIDATE_BTREE ( idTree, recordCount, false, 0, 99999 );

    //-----------------------------------------------------------------------
    printf ( "\nTEST 18\n" );
    printf ( "\nTest random allocation/deallocation of memory.\n" );
    printf ( "\nAt the beginning of the test:\n" );
    PRINT_TREE ( idTree, printFunction );
    VALIDATE_BTREE ( idTree, recordCount, false, 0, 99999 );

    //
    //  Allocate the configured number of chunks of memory and save all the
    //  pointers.
    //
    const int RANDOM_TEST_COUNT     = 50;
    const int MAX_MEMORY_CHUNK_SIZE = 64;

    void* ptrs[RANDOM_TEST_COUNT];
    void* ptr;
    int   size;

    srand ( 1 );

    for ( i = 0; i < RANDOM_TEST_COUNT; ++i )
    {
        size = rand() % MAX_MEMORY_CHUNK_SIZE;
        ptr = sm_malloc ( size );
        ptrs[i] = ptr;
        memset ( ptr, i, size );
        printf ( "----> Allocated block %d of %d bytes at %p[0x%lx]\n",
                 i, size, ptr, toOffset(ptr) );
    }
    //
    //  Now free up most of the chunks of memory in a random order.  Note that
    //  this loop will free random chunks of memory until there are less than
    //  the lower threshold chunks left.  We quit at that point because it
    //  could take a very long time to randomly free the last few chunks of
    //  memory.
    //
    int pointerIndex = 0;
    int chunkCount = RANDOM_TEST_COUNT;
    int chunkLowerLimit = RANDOM_TEST_COUNT / 10;

    while ( chunkCount > chunkLowerLimit )
    {
        pointerIndex = rand();
        // printf ( "<----   Rand generated %'d\n", pointerIndex );
        pointerIndex %= RANDOM_TEST_COUNT;
        ptr = ptrs[pointerIndex];
        if ( ptr != 0 )
        {
            printf ( "<---- Freeing block %d[%d] at %p[0x%lx]\n",
                     pointerIndex, chunkCount, ptr, toOffset(ptr) );

            LOG ( "<---- Memory by Size...\n" );
            btree_print ( &sysControl->availableMemoryBySize, 0 );
            LOG ( "<---- Memory by Offset...\n" );
            btree_print ( &sysControl->availableMemoryByOffset, 0 );

            sm_free ( ptr );
            ptrs[pointerIndex] = 0;
            --chunkCount;
        }
    }
    //
    //  Now free up any chunks that we didn't get in our random free loop
    //  above.
    //
    for ( i = 0; i < RANDOM_TEST_COUNT; ++i )
    {
        ptr = ptrs[i];
        if ( ptr != 0 )
        {
            printf ( "<---- Cleaning up block %d at %p[0x%lx]\n",
                     i, ptr, toOffset(ptr) );
            sm_free ( ptr );
        }
    }
    printf ( "\nAt the end of the test:\n" );
    PRINT_TREE ( idTree, printFunction );
    VALIDATE_BTREE ( idTree, recordCount, false, 0, 99999 );

    dumpSM();

    vsi_core_close();

    //
    //  Everything is finished so terminate the btree tests.
    //
    return 0;
}


/*! @} */

// vim:filetype=c:syntax=c
