/*
    Copyright (C) 2017, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


/*!----------------------------------------------------------------------------

    @file      $File:$

    This file contains the C language wrappers for Python to the VSI API
    functions.

-----------------------------------------------------------------------------*/

/*! @{ */


#include <Python.h>
//
//  Define the following symbol to get debugging output.  Note: You may get
//  some compiler warnings about multiple definitions when you do this.  They
//  can be safely ignored.
//
// #define VSI_DEBUG
#undef VSI_DEBUG

#include <vsi.h>
#include <vsi_core_api.h>
#include <signals.h>

#ifndef LOG
    #define LOG printf
#endif


//
//  Declare the local Python objects we need in this file.
//
static PyObject* VsiError;

//
//  Declare the local functions that implement the python to C wrappers.
//
static PyObject* vsi_insertSignalData ( PyObject* self, PyObject* args );
static PyObject* vsi_getSignalData    ( PyObject* self, PyObject* args );
static PyObject* vsi_flushSignalData  ( PyObject* self, PyObject* args );

//
//  Define the data structure that implements the Python to C function mapping.
//
static PyMethodDef VsiMethods[] =
{
    {
        "insertSignalData",
        vsi_insertSignalData,
        METH_VARARGS,
        "Insert a VSI signal data by name."
    },
    {
        "getSignalData",
        vsi_getSignalData,
        METH_VARARGS,
        "Get the VSI signal data by name."
    },
    {
        "flushSignalData",
        vsi_flushSignalData,
        METH_VARARGS,
        "Flush a VSI signal data by name."
    },
    { NULL, NULL, 0, NULL }        // End of table marker
};


// Handle Python 3 and Python 2
#if PY_MAJOR_VERSION >= 3
    #define MOD_ERROR_VAL NULL
    #define MOD_SUCCESS_VAL(val) val
    #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
    #define MOD_DEF(ob, name, doc, methods) \
        static struct PyModuleDef moduledef = { \
        PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
        ob = PyModule_Create(&moduledef);
#else
    #define MOD_ERROR_VAL
    #define MOD_SUCCESS_VAL(val)
    #define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
    #define MOD_DEF(ob, name, doc, methods) \
        ob = Py_InitModule3(name, methods, doc);
#endif


/*!----------------------------------------------------------------------------

    P y I n i t _ p y V s i

    @brief Initialize the Python interface to the VSI system.

    This function will initialize the Python interface to the VSI system and
    MUST be called before any other VSI functions are called.

    @param[in]  None

    @return 0 - Error occurred (exception thrown)
           ~0 - Success

-----------------------------------------------------------------------------*/
MOD_INIT ( vsi_py )
{
    PyObject* m;

    //
    //  Go create the vsi_py module object.
    //
    MOD_DEF ( m, "vsi_py", NULL, VsiMethods )
    if ( m == NULL )
    {
        return MOD_ERROR_VAL;
    }
    //
    //  Create the "error" object for our module and add it to the module
    //  object.
    //
    VsiError = PyErr_NewException ( "vsi.error", NULL, NULL );
    Py_INCREF ( VsiError );
    PyModule_AddObject ( m, "error", VsiError );

    //
    //  Go initialize the VSI subsystem and handle possible errors.
    //
    LOG ( "Initializing the VSI subsystem...\n" );

    vsi_initialize ( false );

    return MOD_SUCCESS_VAL ( m );
}


/*!----------------------------------------------------------------------------

    v s i _ i n s e r t S i g n a l

    @brief Insert a signal into the VSI system using it's domain and name.

    This function will accept 3 arguments that are the domain and name of the
    signal and the integer data value of the signal.  This information will be
    passed to the VSI system for storage and later retrieval.

    Python usage:

        status = insertSignalData ( domain, name, value )

    @param[in] domain - The domain of the signal.
    @param[in] name - The ASCII CAN name of the signal.
    @param[in] value - The unsigned long integer value of the signal (if any).

    @return status - 0 = Success
                    ~0 = Errno

-----------------------------------------------------------------------------*/
static PyObject* vsi_insertSignalData ( PyObject* self, PyObject* args )
{
    const char*   VSSname = NULL;
    unsigned int  domain  = 0;
    unsigned int  signal  = 0;
    unsigned long value   = 0;
    int           status  = 0;
    vsi_result    result;

    //
    //  Go get the input arguments from the user's function call.
    //
    status = PyArg_ParseTuple ( args, "IIsk", &domain, &signal, &VSSname, &value );
    if ( ! status )
    {
        return PyLong_FromLong ( status );
    }
    //
    //  If we are debugging, output what it is we are doing.
    //
    LOG ( "Inserting signal [%u-%s] with value 0x%lx\n", domain, VSSname,
          (unsigned long)value );
    //
    //  Build the parameter object for the call to VSI that follows.
    //
    result.domainId   = domain;
    result.signalId   = signal;
    result.name       = (char*)VSSname;
    result.data       = (char*)value;
    result.dataLength = sizeof(unsigned long);
    result.status     = 0;

    //
    //  Display the input parameters we received if debug is enabled.
    //
    LOG ( "Calling VSI insert with:\n" );
    LOG ( "  Domain Id: %u\n",    result.domainId );
    LOG ( "  Signal Id: %u\n",    result.signalId );
    LOG ( "       Name: [%s]\n ", result.name );
    LOG ( "   Data Len: %lu\n",   result.dataLength );
    LOG ( "       Data: 0x%lx\n", (unsigned long)result.data );
    LOG ( "     Status: %d\n",    result.status );

    //
    //  If the user did not specify the domain, print an error message and
    //  quit.
    //
    if ( domain == 0 )
    {
        printf ( "Error: The domain must be specified - aborting\n" );
        return PyLong_FromLong ( EINVAL );
    }
    //
    //  Go insert this signal in the VSI subsystem.
    //
    //  If the signal was specified, use that as the lookup key.
    //
    if ( signal != 0 )
    {
        status = vsi_insert_signal ( &result );
    }
    //
    //  Else, if the signal is not present but the name is a non-empty string
    //  then use the name as the lookup key.
    //
    else if ( VSSname != NULL && *VSSname != '\0' )
    {
        status = vsi_insert_signal_by_name ( &result );
    }
    //
    //  If neither the signal nor the name was supplied, it is an error so
    //  complain and quit.
    //
    else
    {
        printf ( "Error: Either the signal or the name must be specified - "
                 "aborting\n" );
        return PyLong_FromLong ( EINVAL );
    }
    //
    //  Display the output parameters from the call if debug is enabled.
    //
    LOG ( "\nReturned[%d] from VSI insert with:\n", status );
    LOG ( "  Domain Id: %u\n",    result.domainId );
    LOG ( "  Signal Id: %u\n",    result.signalId );
    LOG ( "       Name: [%s]\n",  result.name );
    LOG ( "   Data Len: %lu\n",   result.dataLength );
    LOG ( "       Data: 0x%lx\n", (unsigned long)result.data );
    LOG ( "     Status: %d\n",    result.status );

    //
    //  Return the status from the above call to the Python caller.
    //
    return PyLong_FromLong ( status );
}


/*!----------------------------------------------------------------------------

    v s i _ g e t S i g n a l D a t a

    @brief Fetch the signal data from the VSI system using it's name.

    This function will accept 4 arguments that are the domain and name of the
    desired signal and 2 flags indicating whether to wait for a signal if
    there is no data currently, and whether the caller wants the oldest or
    newest data in the signal list.  The signal with the specified domain and
    name will then be looked up.

    If the user wants to wait for data and there is no data currently in the
    signal list, then the call will hang until data is available.  If the user
    does not want to wait and there is no data available, a return code of
    "ENODATA" (61) will be returned.  The "wait" flag is a boolean with a
    value of "true" indicating that we should wait for data.

    If the user specifies the "no-wait" option and there is no data available,
    the contents of the returned "value" variable is undefined.

    If the user specifies that he wants the oldest signal data item, then the
    first item on the signal list will be returned to him and then that item
    will be removed from the list.

    If the user specifies that he wants the newest signal data item, then the
    last item on the signal list will be returned to him and the list left
    unchanged.

    Note that fetching the "newest" will not alter the signal list whereas
    fetching the "oldest" item will cause that item to be removed from the
    list.  The "oldest" flag is a boolean with a value of "true" indicating
    that we should return the oldest data item in the list.

    Also note that if there is only one data item currently in the list of the
    specified signal the "newest" and "oldest" calls will return the same data
    item but whether or not that item is removed from the list depends on
    which age flag was specified in the call.

    Python usage:

        status, value = getSignalData ( domain, name, wait, oldest )

    The return value is a 2 item tuple consisting of "(status, value)".  If
    there was a error (indicated by a non-zero, status), the contents of the
    "value" return item is indeterminate and should not be accessed.

    @param[in] domain - The domain of the desired signal.
    @param[in] name - The ASCII CAN name of the desired signal.
    @param[in] wait - Whether to wait for data or not (true = wait)
    @param[in] oldest - Whether to return the oldest or newest data (true = oldest)

    @return status - The completion status of the VSI core call
    @return value - The signal data value found.

-----------------------------------------------------------------------------*/
static PyObject* vsi_getSignalData ( PyObject* self, PyObject* args )
{
    const char*   VSSname    = NULL;
    domain_t      domain     = 0;
    signal_t      signal     = 0;
    unsigned long data       = 0;
    unsigned long dataLength = sizeof(data);
    unsigned char wait       = 0;
    unsigned char oldest     = 0;
    int           status     = 0;

    //
    //  Go get the input arguments from the user's function call.
    //
    status = PyArg_ParseTuple ( args, "IIsbb", &domain, &signal, &VSSname,
                                &wait, &oldest );
    if ( ! status )
    {
        return PyLong_FromLong ( status );
    }
    //
    //  If the user did not specify the domain, print an error message and
    //  quit.
    //
    if ( domain == 0 )
    {
        printf ( "Error: The domain must be specified - aborting\n" );
        return PyLong_FromLong ( EINVAL );
    }
    //
    //  If the signal ID was not specified...
    //
    if ( signal == 0 )
    {
        //
        //  If the signal name was specified then go lookup the signal by
        //  name.
        //
        if ( VSSname != NULL && *VSSname != '\0' )
        {
            status = vsi_name_string_to_id ( domain, VSSname, &signal );
            if ( status != 0 )
            {
                printf ( "Error: Looking up the signal name returned: %d - "
                         "aborting\n", status );
                PyErr_Format ( PyExc_TypeError,
                               "No signal name entry found for [%s]", VSSname );
                return PyLong_FromLong ( EINVAL );
            }
        }
        //
        //  If neither the signal nor the name was supplied, it is an error so
        //  complain and quit.
        //
        else
        {
            printf ( "Error: Either the signal or the name must be specified - "
                     "aborting\n" );
            return PyLong_FromLong ( EINVAL );
        }
    }
    //
    //  If we are debugging, output what it is we are doing.
    //
    LOG ( "\nCalling VSI fetch with:\n" );
    LOG ( "  Domain Id: %u\n",    domain );
    LOG ( "  Signal Id: %d\n",    signal );
    LOG ( "   VSS Name: [%s]\n",  VSSname );
    LOG ( "       Wait: %d-%s\n", wait, wait ? "Wait" : "No Wait" );
    LOG ( "     Oldest: %d-%s\n", oldest, oldest ? "Oldest" : "Newest" );

    //
    //  Call the appropriate low level function depending on whether we are
    //  fetching the oldest or newest data value from the signal.
    //
    if ( oldest )
    {
        status = sm_fetch ( domain, signal, &dataLength, &data, wait );
    }
    else
    {
        status = sm_fetch_newest ( domain, signal, &dataLength, &data, wait );
    }
    LOG ( "\nReturned[%d] from VSI fetch with:\n", status );
    if ( status == 0 )
    {
        LOG ( "   Data Len: %lu\n",   dataLength );
        LOG ( "       Data: 0x%lx\n", data );
    }
    //
    //  Return the status from the above call to the Python caller.
    //
    LOG ( "vsi_getSignalData returning status: %d, dataLength: %lu, data: 0x%lx\n",
          status, dataLength, data );

    return Py_BuildValue ( "(ii)", status, data );
}


/*!----------------------------------------------------------------------------

    v s i _ f l u s h S i g n a l

    @brief Delete all signal values for the specified signal name.

    This function will take the domain and name of the signal and delete all
    values stored in the VSI database associated with that signal domain and
    name.  Note that the signal definition itself will not be deleted, only
    the data records associated with that signal.

    Python usage:  status = flushSignalData ( domain, name )

    @param[in] domain - The domain of the signal.
    @param[in] name - The ASCII CAN name of the signal.

    @return status - 0 = Success
                    ~0 = Errno

-----------------------------------------------------------------------------*/
static PyObject* vsi_flushSignalData ( PyObject* self, PyObject* args )
{
    const char*   VSSname = NULL;
    unsigned long domain  = 1;
    unsigned long signal  = 0;
    int           status  = 0;
    vsi_result    result;

    //
    //  Go get the input arguments from the user's function call.
    //
    status = PyArg_ParseTuple ( args, "IIs", &domain, &signal, &VSSname );
    if ( ! status )
    {
        return PyLong_FromLong ( status );
    }
    //
    //  If we are debugging, output what it is we are doing.
    //
    LOG ( "Flushing all data for domain: %lu, signal: %lu, name[%s]\n",
          domain, signal, VSSname );
    //
    //  Build the parameter object for the call to VSI that follows.
    //
    result.domainId   = domain;
    result.signalId   = signal;
    result.name       = (char*)VSSname;
    result.data       = 0;
    result.dataLength = sizeof(result.data);
    result.status     = 0;

    //
    //  Display the input parameters we received if debug is enabled.
    //
    LOG ( "Calling vsi_flush_signal_by_name with:\n" );
    LOG ( "  Domain Id: %d\n",   result.domainId );
    LOG ( "  Signal Id: %d\n",   result.signalId );
    LOG ( "       Name: [%s]\n", result.name );
    LOG ( "       Data: %lu\n",  (unsigned long)result.data );
    LOG ( "   Data Len: %lu\n",  result.dataLength );
    LOG ( "     Status: %d\n",   result.status );

    //
    //  If the user did not specify the domain, print an error message and
    //  quit.
    //
    if ( domain == 0 )
    {
        printf ( "Error: The domain must be specified - aborting\n" );
        return PyLong_FromLong ( EINVAL );
    }
    //
    //  Go flush this signal in the VSI data store.
    //
    //  If the signal was specified, use that as the lookup key.
    //
    if ( signal != 0 )
    {
        status = vsi_flush_signal ( domain, signal );
    }
    //
    //  Else, if the signal is not present but the name is a non-empty string
    //  then use the name as the lookup key.
    //
    else if ( VSSname != NULL && *VSSname != '\0' )
    {
        status = vsi_flush_signal_by_name ( domain, VSSname );
    }
    //
    //  If neither the signal nor the name was supplied, it is an error so
    //  complain and quit.
    //
    else
    {
        printf ( "Error: Either the signal or the name must be specified - "
                 "aborting\n" );
        return PyLong_FromLong ( EINVAL );
    }
    //
    //  Display the output parameters from the call if debug is enabled.
    //
    LOG ( "\nReturned[%d] from vsi_flush_signal_by_name with:\n", status );
    LOG ( "  Domain Id: %u\n",   result.domainId );
    LOG ( "  Signal Id: %u\n",   result.signalId );
    LOG ( "       Name: [%s]\n", result.name );
    LOG ( "       Data: %lu\n",  (unsigned long)result.data );
    LOG ( "   Data Len: %lu\n",  result.dataLength );
    LOG ( "     Status: %d\n",   result.status );

    //
    //  Return the status from the above call to the Python caller.
    //
    return PyLong_FromLong ( status );
}


/*! @} */

// vim:filetype=c:syntax=c

