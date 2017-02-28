
#define IPATH python PYTHON_VERSION
#define IFILE Python.h

#define __header(x) #x
#define _header(p,f) __header(p/f)
#define header(p,f) _header(p,f)
#include header(IPATH,IFILE)


//
//  Define the following symbol to get debugging output.  Note: You may get
//  some compiler warnings about multiple definitions when you do this.  They
//  can be safely be ignored.
//
#undef VSI_DEBUG

#include <vsi.h>


static vsi_handle handle = 0;

static PyObject* VsiError;

static PyObject* vsi_fireSignalByName ( PyObject* self, PyObject* args );

static PyMethodDef VsiMethods[] =
{
    { "fireSignalByName", vsi_fireSignalByName, METH_VARARGS, "Fire a VSI signal."},

    {NULL, NULL, 0, NULL}        /* Sentinel */
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
MOD_INIT(vsi_py)
{
    PyObject* m;

    //
    //  Go create the vsi_py module object.
    //
    MOD_DEF(m, "vsi_py", NULL, VsiMethods)
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

    handle = vsi_initialize ( false );

    if ( handle == NULL )
    {
        LOG ( "Error: Unable to initialize VSI\n" );
        PyErr_Format ( PyExc_SystemError, "Unable to initialize VSI\n" );
        return MOD_ERROR_VAL;
    }
    return MOD_SUCCESS_VAL(m);
}


//
//  Define the structure for creating the signal name map.
//
typedef struct NameMap_t
{
    const char* CANname;
    const char* VSSname;

}   NameMap_t;


//
//  Define the signal name map that will allow us to translate to/from CAN and
//  VSS names.
//
static NameMap_t nameMap[] =
{
    { "RainSensorActiveHS_HS",    "Signal.Body.Rainsensor.Intensity" },
    { "WheelSpeedReR_HS",         "Signal.Drivetrain.Transmission.Speed" },
    { "WheelSpeedFrR_HS",         "Signal.Drivetrain.Transmission.Speed" },
    { "WheelSpeedReL_HS",         "Signal.Drivetrain.Transmission.Speed" },
    { "WheelSpeedReR_HS",         "Signal.Drivetrain.Transmission.Speed" },
    { "EngineOilLevelMsg_HS",     "Not.Yet.Assigned" },
    { "SuspensionHeightRR_HS",    "Attribute.Chassis.Height" },
    { "AmbientTemp_JLR_HS",       "Not.Yet.Assigned" },
    { "AmbientAirPressure_HS",    "Not.Yet.Assigned" },
    { "LastAlarmCause_HS",        "Not.Yet.Assigned" },
    { "DoorStatusMS_MS",          "Signal.Cabin.Door.Row1.Right.IsOpen" },
    { "PowerModeCompMS_MS",       "Signal.Drivetrain.InternalCombustionEngine.Power" },
    { "WiperStatusMS_MS",         "Signal.Body.Windshield.Front.Wiper.Status" },
    { "EngineSpeed_MS",           "Not.Yet.Assigned" },
    { "ThrottlePosition_MS",      "Signal.Chassis.Accelerator.PedalPosition" },
    { "GearLeverPositionMS_MS",   "Signal.Drivetrain.Transmission.Gear" },
    { "SteeringWheelAngle_MS",    "Attribute.Cabin.SteeringWheel.Position" },
    { "DrivSeatBeltBcklState_MS", "Signal.Cabin.Seat.Row1.Pos1.IsBelted" },
    { "FuelLevelIndicatedMS_MS",  "Signal.Drivetrain.FuelSystem.Level" },
    { "DriverWindowPosition_MS",  "Signal.Cabin.Door.Row1.Left.Window.Position" },
    { "AlarmMode_MS",             "Not.Yet.Assigned" },
    { "PassWindowPosition_MS",    "Signal.Cabin.Door.Row1.Right.Window.Position" }
};

//
//  Define the number of names defined in our name map above.
//
static const int mapCount = sizeof(nameMap) / sizeof(NameMap_t);


/*!----------------------------------------------------------------------------

    v s i T r a n s l a t e N a m e

	@brief Translate a CAN signal name to a VSS signal name.

	This function will translate a CAN signal name to a VSS signal name.

	@param[in] CANname - The ASCII CAN name string.

	@return VSSname - The corresponding VSS name string.
            NULL - If the CAN name could not be found in the translate table.

-----------------------------------------------------------------------------*/
static const char* vsiTranslateName ( const char* CANname )
{
    //
    //  Search through the signal name mapping table...
    //
    for ( int i = 0; i < mapCount; ++i )
    {
        //
        //  If this mapping entry matches the CAN signal name we were give,
        //  return the corresponding VSS signal name from the mapping table.
        //
        if ( strncmp ( CANname, nameMap[i].CANname, 80 ) == 0 )
        {
            return nameMap[i].VSSname;
        }
    }
    //
    //  If we did not find the given CAN signal name in our table, complain
    //  and return a NULL to the caller.
    //
    LOG ( "vsiTranslateName called with unknown CAN name[%s]\n", CANname );
    return NULL;
}


/*!----------------------------------------------------------------------------

    v s i _ f i r e S i g n a l B y N a m e

	@brief Send a signal to the VSI system using it's name.

	This function will accept 2 arguments that are the name of the signal and
    the integer value of the signal.  This information will be passed to the
    VSI system for storage and later retrieval.

	@param[in] name - The ASCII name of the signal.
	@param[in] value - The signed integer value of the signal (if any).

	@return status - 0 = Success
                    ~0 = Errno

-----------------------------------------------------------------------------*/
static PyObject* vsi_fireSignalByName ( PyObject* self, PyObject* args )
{
    const char* CANname;
    const char* VSSname;
    long int    value;
    int         status;
    vsi_result  result;

    //
    //  Go get the input arguments from the user's function call.
    //
    if ( ! PyArg_ParseTuple ( args, "sl", &CANname, &value ) )
    {
        return NULL;
    }
    //
    //  If we are debugging, output what it is we are doing.
    //
    LOG ( "Firing signal [%s] with value %ld\n", CANname, value );

    //
    //  Go translate the caller's CAN signal name to a VSS signal name.
    //
    VSSname = vsiTranslateName ( CANname );

    //
    //  If we did not find a signal name mapping, generate an exception to
    //  Python.
    //
    if ( VSSname == NULL )
    {
        // PyErr_SetString ( VsiError, "No CAN/VSS mapping entry found" );
        PyErr_Format ( PyExc_TypeError, "No CAN/VSS mapping entry found for [%s]",
                       CANname );
        return NULL;
    }
    //
    //  If we are debugging, tell the user what we mapped his name to.
    //
    LOG ( "    Translated CAN[%s] to VSS[%s]\n", CANname, VSSname );

    //
    //  Build the parameter object for the call to VSI that follows.
    //
    result.context    = handle;
    result.domainId   = VSS;
    result.signalId   = 0;
    result.name       = (char*)VSSname;
    result.data       = (char*)value;
    result.dataLength = sizeof(long int);
    result.status     = 0;

    //
    //  Go fire this signal in the VSI subsystem.
    //
    status = vsi_fire_signal_by_name ( handle, &result );

    //
    //  Return the status from the above call to the Python caller.
    //
    return PyLong_FromLong ( status );
}
