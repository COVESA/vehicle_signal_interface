set(SRC
    vsi.c
    vsi_list.c
)

add_library(vsi-api SHARED ${SRC})
install(TARGETS vsi-api LIBRARY DESTINATION usr/lib)

