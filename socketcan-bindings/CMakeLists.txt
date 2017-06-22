set(SRC
    vsi-socketcand.c
    can-signals.c
    genivi-demo-signals.c
)

add_executable(vsi-socketcand ${SRC})
target_link_libraries(vsi-socketcand ${CMAKE_THREAD_LIBS_INIT} vsi)

