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
target_link_libraries(vsi pthread)
install(TARGETS vsi LIBRARY DESTINATION lib)

add_executable(btreeTests btreeTests.c)
target_link_libraries(btreeTests pthread vsi)

add_executable(dump dump.c)
target_link_libraries(dump pthread vsi)

add_executable(fetch fetch.c)
target_link_libraries(fetch pthread vsi)

add_executable(flush flush.c)
target_link_libraries(flush pthread vsi)

add_executable(importVSS importVSS.c)
target_link_libraries(importVSS pthread vsi)

add_executable(insert insert.c)
target_link_libraries(insert pthread vsi)

add_executable(readRecord readRecord.c)
target_link_libraries(readRecord pthread vsi)

add_executable(sample sample.c)
target_link_libraries(sample pthread vsi)

add_executable(smTests smTests.c)
target_link_libraries(smTests pthread vsi)

add_executable(writeRecord writeRecord.c)
target_link_libraries(writeRecord pthread vsi)

