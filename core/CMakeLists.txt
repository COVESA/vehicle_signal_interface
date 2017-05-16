set(SRC
    btree.c
    sharedMemory.c
    sharedMemoryLocks.c
    utils.c
    vsi_core_api.c
)

add_library(vsi-core SHARED ${SRC})
target_link_libraries(vsi-core pthread)
install(TARGETS vsi-core LIBRARY DESTINATION usr/lib)

