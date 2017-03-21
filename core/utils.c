/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*!----------------------------------------------------------------------------

    @file      utils.c

    This file contains the utility functions that are used by various parts of
    the main shared memory test code.

    There are dump functions defined here that can be called by the code
    during debugging to gain insight into the state and structure of the
    various shared memory segment data structures.

-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <stdbool.h>
#include <limits.h>

#include "sharedMemory.h"
#include "utils.h"


/*! @{ */

//
//  WARNING!!!: All references (and pointers) to data in the VSI signal buffer
//  are performed by using the offset from the shared memory segment base
//  address.  All of these references need to be relocatable references so
//  this will work in multiple processes that have their shared memory
//  segments mapped to different base addresses.
//


#if 0
/*!----------------------------------------------------------------------------

    d u m p A l l S i g n a l s

    @brief Dump all of the signals currently defined.

    This function will dump all of the signals currently defined.  It
    traverses all of the signal lists and dumps them in the order in which
    they are found.  This order is determined by the comparison function
    defined for the signal list B-tree (usually the domain & ID of the
    signal).

    @param[in] maxSignals - The maximum number of signals to dump per list.

    @return None

-----------------------------------------------------------------------------*/
void dumpAllSignals ( int maxSignals )
{
    signalList_t* signals;

    //
    //  If the system has not finished initializing yet, just ignore this call.
    //
    if ( ! smControl->systemInitialized )
    {
        return;
    }

    btree_iter iter = btree_iter_begin ( &smControl->signals );

    while ( ! btree_iter_at_end ( iter ) )
    {
        signals = btree_iter_data ( iter );

        dumpSignalList ( signals, maxSignals );

        btree_iter_next ( iter );
    }
    btree_iter_cleanup ( iter );

    return;
}
#endif


/*!-----------------------------------------------------------------------

    d u m p S i g n a l L i s t

    @brief Dump the contents of the specified signal list.

    This function will Dump the contents of a single signal list.  This will
    also dump the individual signals contained in this list.

    @param[in] signalList - The address of the signal list to be dumped
    @param[in] maxSignals - The maximum number of signals to display in each
                            signal list.
    @return None

------------------------------------------------------------------------*/
void dumpSignalList ( signalList_t* signalList, int maxSignals )
{
    //
    //  If the system has not finished initializing yet, just ignore this call.
    //
    if ( ! smControl->systemInitialized )
    {
        return;
    }
    //
    //  If there are some signals in this signal list...
    //
    if ( signalList->currentSignalCount > 0 )
    {
        //
        //  Display the address of this signal list and it's offset within the
        //  shared memory segment.
        //
        LOG ( "Signal List %p[0x%lx]:\n", signalList, toOffset ( signalList ) );

        //
        //  If the head offset indicates that this is the end of the signal
        //  list, display a signal saying we hit the end of the list.
        //
        if ( signalList->head == END_OF_LIST_MARKER )
        {
            LOG ( "Head offset............: End of List Marker\n" );
        }
        //
        //  Otherwise, display the head offset value.  This is the offset in
        //  the segment of the first data signal record.
        //
        else
        {
            LOG ( "Head offset............: 0x%lx[%lu]\n", signalList->head,
                  signalList->head );
        }
        //
        //  If the tail offset indicates that this is the end of the signal
        //  list, display a signal saying we hit the end of the list.
        //
        if ( signalList->tail == END_OF_LIST_MARKER )
        {
            LOG ( "Tail offset............: End of List Marker\n" );
        }
        //
        //  Otherwise, display the tail offset value.  This is the offset in
        //  the segment of the start of the last data signal record.
        //
        else
        {
            LOG ( "Tail offset............: 0x%lx[%lu]\n", signalList->tail,
                  signalList->tail );
        }
        //
        //  Display the number of messages currently stored in this hash
        //  bucket.
        //
        LOG ( "Message count..........: %'lu\n", signalList->currentSignalCount );

        //
        //  Display the number of bytes of memory that have been allocated to
        //  this signal list for the storage of signals.
        //
        LOG ( "Total signal size......: %lu[0x%lx]\n", signalList->totalSignalSize,
              signalList->totalSignalSize );

        //
        //  Go dump the contents of the signal list in this entry.
        //
        dumpSignals ( signalList, maxSignals );
    }
    return;
}


/*!-----------------------------------------------------------------------

    d u m p S i g n a l s

    @brief Dump the contents of the specified shared memory signal list.

    This function will dump all of the signals in the specified signals list.
    The actual binary contents of each message is also dumped.

    @param[in] signalList - The address of the signalList to be dumped.
    @param[in] maxSignals - The maximum number of signals to dump for this
                            signal list.
    @return None

------------------------------------------------------------------------*/
void dumpSignals ( signalList_t* signalList, int maxSignals )
{
#ifdef VSI_DEBUG
    int i = 0;
#endif

    //
    //  If the system has not finished initializing yet, just ignore this call.
    //
    if ( ! smControl->systemInitialized )
    {
        return;
    }
    //
    //  If the user did not specify a maximum signal count, set it to a large
    //  number.
    //
    if ( maxSignals == 0 )
    {
        maxSignals = INT_MAX;
    }
    //
    //  Get the offset to the first signal in this list.
    //
    offset_t signalOffset = signalList->head;

    //
    //  If this signal list is empty, just silently quit.
    //
    if ( signalList->currentSignalCount <= 0 )
    {
        return;
    }
    //
    //  Get an actual pointer to the first signal in this list.  This is
    //  the signal that is pointed to by the "head" offset.
    //
    signalData_t* signal = toAddress ( signalOffset );

    //
    //  Display the signal list address.
    //
    LOG ( "    Signal list [%p]:\n", signalList );

    //
    //  Display the domain associated with this signal.
    //
    LOG ( "      Domain: %'lu\n", signalList->domain );

    //
    //  Display the key value that was associated with this signal.
    //
    LOG ( "      Key:    %'lu\n", signalList->key );

    //
    //  Repeat until we reach the end of the signal list in this bucket.
    //
    while ( signalOffset != END_OF_LIST_MARKER )
    {
        //
        //  Display the size of this signal in bytes.
        //
        LOG ( "      %'d - Signal data size...: %'lu\n", ++i, signal->messageSize );

        //
        //  If the "next" signal offset indicates that this is the end of the
        //  signal list, display a message saying we hit the end of the list.
        //
        signalOffset = signal->nextMessageOffset;
        if ( signalOffset == END_OF_LIST_MARKER )
        {
            LOG ( "      Next signal offset.: End of List Marker\n" );
        }
        //
        //  Otherwise, display the next signal offset value.  This is the
        //  offset in the signal list where the next logical signal in the
        //  list is located.
        //
        else
        {
            LOG ( "       Next signal offset.: %'lu\n", signalOffset );
        }
        //
        //  Go dump the contents of the data field for this signal.
        //
        HexDump ( signal->data, signal->messageSize, "Signal Data", 6 );

        //
        //  If we have reached the maximum number of signals the caller asked
        //  us to disply, break out of this loop and quit.
        //
        if ( --maxSignals == 0 )
        {
            break;
        }
        //
        //  Get the address of where the next signal in the list is located.
        //
        signal = toAddress ( signalOffset );
    }
    return;
}


/*!-----------------------------------------------------------------------

    d u m p S e m a p h o r e

    @brief Dump the fields of a semaphore.

    This function will dump all of the fields of the specified semaphore that
    make sense.  The mutex and condition variable are not dumped as we have no
    explicit information about what is inside them.

    @param[in] semaphore - The address of the semaphore to be dumped.

------------------------------------------------------------------------*/
void dumpSemaphore ( semaphore_p semaphore )
{
#ifdef DUMP_SEMAPHORE
    LOG ( "semaphore: %3p\n", (void*)((long)&semaphore->mutex % 0x1000 ) );

    //
    //  Display our count values.
    //
    LOG ( "   message count.........: %'d\n", semaphore->messageCount );
    LOG ( "   waiter count..........: %'d\n", semaphore->waiterCount );

    //
    //  Display the semaphore data...
    //
    LOG ( "   semaphore.mutex.lock..: %d\n", semaphore->mutex.__data.__lock );
    LOG ( "   semaphore.mutex.count.: %d\n", semaphore->mutex.__data.__count );
    LOG ( "   semaphore.mutex.owner.: %d\n", semaphore->mutex.__data.__owner );
    LOG ( "   semaphore.mutex.nusers: %d\n", semaphore->mutex.__data.__nusers );
    LOG ( "   semaphore.mutex.kind..: %d\n", semaphore->mutex.__data.__kind );

#endif
    return;
}


/******************************************************************************

        H e x D u m p

    This function produces a hex/ASCII dump of the input data similar to:

    Optional title string (25 bytes @ 0x86e9820):
    0000   00 00 00 00 0B 00 00 00 FF FF FF FF 64 65 62 75  ............debu
    0016   67 4C 65 76 65 6C 3A 00 98                       gLevel:..

    Note that the input does not need to be a string, just an array of bytes.
    The dump will be output with 16 bytes per line and have the ASCII
    representation of the data appended to each line.  Unprintable characters
    are printed as a period.

    The title, outputWidth, and leadingSpaces are optional arguments that can
    be left off by using one of the helper macros.

    The output from calling this function will be generated on the "stdout"
    device.

    The maximum length of a hex dump is specified by the constant below and
    all dumps will be truncated to this length if they are longer than this
    maximum value.

    This function should be used as follows:

        HexDump ( data, length, "Title", 0 );

    or in this case:

        HEX_DUMP_TL ( data, length, "Title", 0 );

    Input Parameters:

        data - The address of the data buffer to be dumped
        length - The number of bytes of data to be dumped
        title - The address of an optional title string for the dump
        leadingSpaces - The number of spaces to be added at the beginning of
                        each line.

    Output Data:  None

    Return Value: None

******************************************************************************/

//
//    The following macro is designed for debugging this function.  To enable
//    it, define this as a synonym for "printf".
//
// #define XPRINT printf
#define XPRINT(...)

#define MAX_DUMP_SIZE ( 1024 )

//
//  The following macros are defined in the header file to make it easier to
//  call the HexDump function with various combinations of arguments.
//
// #define HEX_DUMP(   data, length )         HexDump ( data, length, "", 0 )
// #define HEX_DUMP_T( data, length, title )  HexDump ( data, length, title, 0 )
// #define HEX_DUMP_L( data, length, spaces ) HexDump ( data, length, "", spaces )
// #define HEX_DUMP_TL                        HexDump
//

void HexDump ( const char *data, int length, const char *title,
               int leadingSpaces )
{
    unsigned char c;
    int           i;
    int           j;
    int           s                = 0;
    int           outputWidth      = 16;
    int           currentLineWidth = 0;
    int           originalLength   = 0;
    char          asciiString[400] = { 0 };

    XPRINT ( "data(%d): \"%s\", title: %p, width: %d, spaces: %d\n", length,
             data, title, outputWidth, leadingSpaces );
    //
    //    If the length field is unreasonably long, truncate it.
    //
    originalLength = length;
    if ( length > MAX_DUMP_SIZE )
    {
        //
        //    Set the length to the maximum value we will allow.
        //
        length = MAX_DUMP_SIZE;
    }
    //
    //    Output the leader string at the beginning of the title line.
    //
    for ( i = 0; i < leadingSpaces; i++ )
    {
        putchar ( ' ' );
    }
    //
    //    If the user specified a title for the dump, output that title
    //    followed by the buffer size and location.
    //
    if ( title != NULL && strlen(title) != 0 )
    {
        fputs ( title, stdout );
        printf ( " (%d bytes @ %p):\n", originalLength, data );
    }
    //
    //    If the user did not specify a title for the dump, just output the
    //    buffer size and location.
    //
    else
    {
        printf ( "%d bytes @ %p:\n", originalLength, data );
    }
    //
    //    For each byte in the data to be dumped...
    //
    for ( i = 0; i < length; i++ )
    {
        //
        //    If this is the beginning of a new line...
        //
        if ( i % outputWidth == 0 )
        {
            XPRINT ( "New line offset %d", i );

            //
            //    Reset the number of fields output on this line so far.
            //
            currentLineWidth = 0;

            //
            //    If this is not the beginning of the first line, append the
            //    ascii interpretation to the hex dump data at the end of the
            //    line.
            //
            if ( i > 0 )
            {
                asciiString[s] = 0;
                puts ( asciiString );

                asciiString[0] = 0;
                s = 0;
            }
            //
            //    Output the leader string at the beginning of the new line.
            //
            for ( j = 0; j < leadingSpaces; j++ )
            {
                putchar ( ' ' );
            }
            //
            //    Output the data offset field for this new line.
            //
            printf ( "%06d  ", i );
        }
        //
        //    Generate the hex representation for the current data character.
        //
        c = *data++;
        printf ( "%02x ", c );

        //
        //    Generate the ASCII representation of the data character at this
        //    position if it is a printable character.  If it is not, generate
        //    a period in it's place.
        //
        if ( c >= ' ' && c <= '~' )
        {
            asciiString[s++] = c;
        }
        else
        {
            asciiString[s++] = '.';
        }
        //
        //    Increment the width of the line generated so far.
        //
        currentLineWidth++;
    }
    //
    //    We have reached the end of the data to be dumped.
    //
    //    Pad the rest of the fields on this line with blanks so the ASCII
    //    representation will line up with previous lines if it is an
    //    incomplete line.
    //
    for ( i = 0; i < outputWidth - currentLineWidth; i++ )
    {
        fputs ( "   ", stdout );
    }
    //
    //    Output the last line of the display.
    //
    asciiString[s] = 0;
    puts ( asciiString );

    //
    //    If the original length was truncated because it was unreasonably
    //    long then output a message indicating that the dump was truncated.
    //
    if ( originalLength > length )
    {
        printf ( "       ...Dump of %d bytes has been truncated\n", originalLength );
    }
    //
    //    Return to the caller.
    //
    return;
}

#ifdef VSI_DEBUG

//
//  TODO: The following function is for debugging purposes.
//
//  This function will return the time in microseconds since the shared memory
//  segment was first accessed after being initialized.  The time is in
//  microseconds instead of the raw nanosecond form because the nanosecond
//  portion of the time is not very interesting and just clutters up the
//  output.
//
//  The first time this function is called it will set the global base time
//  contained in the shared memory control block.  All subsequent times are
//  then computed relative to this base time.
//
#define NS_PER_SEC ( 1000000000 )
#define NS_PER_US  ( 1000 )

unsigned long getIntervalTime ( void )
{
    unsigned long currentTime;
    struct timespec timeSpec;

    if ( smControl != 0 )
    {
        clock_gettime ( CLOCK_REALTIME, &timeSpec );
        currentTime = timeSpec.tv_sec * NS_PER_SEC + timeSpec.tv_nsec;

        if ( smControl->globalTime == 0 )
        {
            smControl->globalTime = currentTime;
        }
        return ( currentTime - smControl->globalTime ) / NS_PER_US;
    }
    else
    {
        return 0;
    }

}

#endif      // #ifdef VSI_DEBUG


/*! @} */

// vim:filetype=c:syntax=c
