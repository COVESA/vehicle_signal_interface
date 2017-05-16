find_package( PythonLibs ${MIN_PYTHON_VERSION} REQUIRED )

include_directories(${PYTHON_INCLUDE_DIRS})  
set(SRC
    vsi_py.c
)

add_library(vsi_py SHARED ${SRC})
install(TARGETS vsi_py LIBRARY DESTINATION usr/lib)


