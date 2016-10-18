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

#include "utils.h"

#ifdef VSI_DEBUG
#    define LOG printf
#    define HX_DUMP HexDump
#else
#    define LOG(...)
#    define HX_DUMP(...)
#    undef VSI_TRACE
#endif

//
//  Define the following if you'd like the code to dump the semaphore states
//  as it operates.  This is normally disabled as it produces a large volume
//  of dump data.
//
#undef DUMP_SEMAPHORE


sharedMemory_p sharedMemory;		// TEMP - Used by the dump code

/*! @{ */

//
//	WARNING!!!: All references (and pointers) to data in the can message
//	buffer is performed by using the index into the array of messages.  All of
//	these references need to be relocatable references so this will work in
//	multiple processes that have their shared memory segments mapped to
//	different base addresses.
//


/*!-----------------------------------------------------------------------

	d u m p H a s h B u c k e t

	@brief Dump the contents of the specified hash table bucket.

	This function will Dump the contents of a single hash bucket.  This will
	also dump the message list contained in this bucket but not the binary
	data contained in each message.

    Note that hash buckets that do not contain any data will be silently
	ignored.

	The leader argument is designed to allow the caller to specify a string
	that is output at the beginning of each dump line.  This allows the user
	to specify an indentation for the following dump information for instance.

	@param[in] leader - The string to be output at the start of each line
	@param[in] bucketNumber - The bucket index number to be dumped
	@param[in] hashBucket - The address of the hash bucket to be dumped
	@param[in] maxMessages - The maximum number of message to display in each
	                         hash bucket.
	@return None

------------------------------------------------------------------------*/
void dumpHashBucket ( unsigned int bucketNumber, hashBucket_p hashBucket,
                      int maxMessages )
{
	//
	//	If there are some messages in this hash bucket...
	//
	if ( hashBucket->currentMessageCount > 0 )
	{
		//
		//	Display the address of this bucket and it's offset within the
		//	shared memory segment.
		//
		LOG ( "Hash Bucket %'u[%lx]:\n", bucketNumber,
				 (void*)hashBucket - (void*)sharedMemory );
		//
		//	If the head offset indicates that this is the end of the message
		//	list, display a message saying we hit the end of the list.
		//
		if ( hashBucket->head == END_OF_BUCKET_DATA )
		{
			LOG ( "Head offset............: End of List Marker\n" );
		}
		//
		//	Otherwise, display the head offset value.  This is the offset in
		//	the segment of the first data message record.
		//
		else
		{
			LOG ( "Head offset............: %'lu\n", hashBucket->head );
		}
		//
		//	If the tail offset indicates that this is the end of the message
		//	list, display a message saying we hit the end of the list.
		//
		if ( hashBucket->tail == END_OF_BUCKET_DATA )
		{
			LOG ( "Tail offset............: End of List Marker\n" );
		}
		//
		//	Otherwise, display the tail offset value.  This is the offset in
		//	the segment of the start of the last data message record.
		//
		else
		{
			LOG ( "Tail offset............: %'lu\n", hashBucket->tail );
		}
		//
		//	Display the number of messages currently stored in this hash
		//	bucket.
		//
		LOG ( "Message count..........: %'lu\n", hashBucket->currentMessageCount );
		//
		//	Display the generation number of this hash bucket.  The
		//	generation number is incremented each time the messages being
		//	inserted reach the end of the buffer and we have wrapped around
		//	back to the beginning of the buffer.
		//
		LOG ( "Generation number......: %'lu\n", hashBucket->generationNumber );
		//
		//	Display the message sequence number.  This number is the total
		//	number of messages that have been stored in this hash bucket since
		//	the shared memory segment was created.  This can be used as an
		//	indicator of how often this bucket has been used.
		//
		LOG ( "Message sequence number: %'lu\n", hashBucket->messageSequenceNumber );
		//
		//	Display the number of bytes of memory that have been allocated to
		//	this hash bucket for the storage of messages.  This value is
		//	static and does not change as the hash bucket is used.
		//
		LOG ( "Total message size.....: %'lu\n", hashBucket->totalMessageSize );
		//
		//	Display the total size of this hash bucket in bytes.
		//
		LOG ( "Hash bucket size.......: %'d\n", HASH_BUCKET_DATA_SIZE );
		//
		//	Go dump the contents of the message list in this hash bucket.
		//
		dumpMessageList ( hashBucket, maxMessages );
	}
	return;
}


/*!-----------------------------------------------------------------------

	d u m p M e s s a g e L i s t

	@brief Dump the contents of the specified shared memory message list.

	This function will dump all of the messages in the specified message list.
	Only the meta-data of each message is dump.  The actual binary contents of
	each message is not dumped (to keep the output reasonable).

	@param[in] bucketNumber - The bucket index number to be dumped
	@param[in] maxMessages - The maximum number of messages to dump for this
							 hash bucket.
	@return None

------------------------------------------------------------------------*/
void dumpMessageList ( hashBucket_p hashBucket, int maxMessages )
{
#ifdef VSI_DEBUG
	int i = 0;

	//
	//	Get the base address of the message list so that we can display the
	//	list offsets of each message.
	//
	void* baseAddress = hb_getAddress ( hashBucket, 0 );
#endif

	//
	//	Get an actual pointer to the first message in this bucket.  This is
	//	the message that is pointed to by the "head" offset.
	//
	sharedMessage_p message = hb_getAddress ( hashBucket,
											  hb_getHead ( hashBucket ) );
	//
	//	Repeat until we reach the end of the message list in this bucket.
	//
	while ( true )
	{
		//
		//	Display the message number (starting at "1"), the address of this
		//	message and the offset of this message relative to the start of
		//	the message list in this hash bucket.  The first message in the
		//	list will be at offset "0".
		//
		LOG ( "Message number %'d[%p-%'lu]:\n", ++i, message,
				 (void*)message - baseAddress );
		//
		//	Display the domain associated with this message.
		//
		LOG ( "   Domain..............: %'u\n", message->domain );

		//
		//	Display the key value that was associated with this message.
		//
		LOG ( "   Key.................: %'u\n", message->key );

		//
		//	Display the size of this message in bytes.
		//
		LOG ( "   Message size........: %'lu\n", message->messageSize );

		//
		//	If the "next" message offset indicates that this is the end of the
		//	message list, display a message saying we hit the end of the list.
		//
		if ( message->nextMessageOffset == END_OF_BUCKET_DATA )
		{
			LOG ( "   Next message offset.: End of List Marker\n" );
		}
		//
		//	Otherwise, display the head offset value.  This is the offset in
		//	the message list where the next logical message in the list is
		//	located.
		//
		else
		{
			LOG ( "   Next message offset.: %'lu\n",
                  message->nextMessageOffset );
		}
		//
		//	Go dump the contents of the data field for this message.
		//
		HX_DUMP ( message->data, message->messageSize, "Message Data" );

		if ( message->nextMessageOffset == END_OF_BUCKET_DATA ||
			 --maxMessages == 0 )
		{
			break;
		}
		//
		//	Get the address of where the next message in the list is located.
		//
		message =  hb_getAddress ( hashBucket, message->nextMessageOffset );
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
	//	Display our count values.
	//
	LOG ( "   message count.........: %'d\n", semaphore->messageCount );
	LOG ( "   waiter count..........: %'d\n", semaphore->waiterCount );

	//
	//	Display the semaphore data...
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
// #define XPRINT(...) printf ( __VA_ARGS__ )
#define XPRINT(...)

#define MAX_DUMP_SIZE ( 1024 )

//
//	The following macros are defined in the header file to make it easier to
//	call the HexDump function with various combinations of arguments.
//
// #define HEX_DUMP( data, length )           HexDump ( data, length, "", 0 )
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
    int           s = 0;
    int           outputWidth = 16;
    int           currentLineWidth = 0;
    int           originalLength = 0;
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
#ifdef VSI_TRACE
    //
    //  If tracing is enabled, print out the trace indentation string.
    //
    printTraceLeader();
#endif
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
#ifdef VSI_TRACE
            //
            //  If tracing is enabled, print out the trace indentation string.
            //
            printTraceLeader();
#endif
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
#ifdef VSI_TRACE
        //
        //  If tracing is enabled, print out the trace indentation string.
        //
        printTraceLeader();
#endif
		printf ( "       ...Dump of %d bytes has been truncated\n", originalLength );
    }
    //
    //    Return to the caller.
    //
    return;
}


//
//	TODO: The following function is for debugging purposes.
//
//	This function will return the time in microseconds since the shared memory
//	segment was first accessed after being initialized.  The time is in
//	microseconds instead of the raw nanosecond form because the nanosecond
//	portion of the time is not very interesting and just clutters up the
//	output.
//
#define NS_PER_SEC ( 1000000000 )
#define NS_PER_US  ( 1000 )

unsigned long getIntervalTime ( void )
{
	unsigned long currentTime;
	struct timespec timeSpec;

    if ( sharedMemory != 0 )
    {
        clock_gettime ( CLOCK_REALTIME, &timeSpec );
        currentTime = timeSpec.tv_sec * NS_PER_SEC + timeSpec.tv_nsec;

        if ( sharedMemory->globalTime == 0 )
        {
            sharedMemory->globalTime = currentTime;
        }
        return ( currentTime - sharedMemory->globalTime ) / NS_PER_US;
    }
    else
    {
        return 0;
    }

}


/*! @} */

// vim:filetype=c:syntax=c
