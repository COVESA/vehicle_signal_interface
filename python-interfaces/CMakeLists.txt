find_package( PythonLibs ${MIN_PYTHON_VERSION} REQUIRED )

include_directories(${PYTHON_INCLUDE_DIRS})  
set(SRC
    vsi_py.c
)

add_library(vsi_py SHARED ${SRC})
set_target_properties(vsi_py PROPERTIES PREFIX "")
target_link_libraries(vsi_py vsi)
install(TARGETS vsi_py LIBRARY DESTINATION lib)


