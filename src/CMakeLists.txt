set(SRC
    btree.c
    sharedMemory.c
    sharedMemoryLocks.c
    signals.c
    utils.c
    vsi_core_api.c
    vsi.c
)

add_library(vsi SHARED ${SRC})
target_link_libraries(vsi ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS vsi LIBRARY DESTINATION lib)

add_executable(btreeTests btreeTests.c)
target_link_libraries(btreeTests ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(dump dump.c)
target_link_libraries(dump ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(fetch fetch.c)
target_link_libraries(fetch ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(flush flush.c)
target_link_libraries(flush ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(importVSS importVSS.c)
target_link_libraries(importVSS ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(insert insert.c)
target_link_libraries(insert ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(readRecord readRecord.c)
target_link_libraries(readRecord ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(sample sample.c)
target_link_libraries(sample ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(smTests smTests.c)
target_link_libraries(smTests ${CMAKE_THREAD_LIBS_INIT} vsi)

add_executable(writeRecord writeRecord.c)
target_link_libraries(writeRecord ${CMAKE_THREAD_LIBS_INIT} vsi)

