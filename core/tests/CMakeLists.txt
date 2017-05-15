add_executable(dump dump.c)
target_link_libraries(dump pthread vsi-core)

add_executable(fetch fetch.c)
target_link_libraries(fetch pthread vsi-core)

add_executable(flush flush.c)
target_link_libraries(flush pthread vsi-core)

add_executable(insert insert.c)
target_link_libraries(insert pthread vsi-core)

add_executable(readRecord readRecord.c)
target_link_libraries(readRecord pthread vsi-core)

add_executable(writeRecord writeRecord.c)
target_link_libraries(writeRecord pthread vsi-core)

add_executable(btreeTests btreeTests.c)
target_link_libraries(btreeTests pthread vsi-core)

add_executable(smTests smTests.c)
target_link_libraries(smTests pthread vsi-core)

