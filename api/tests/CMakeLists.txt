add_executable(importVSS importVSS.c)
target_link_libraries(importVSS pthread vsi-core vsi-api)

add_executable(sample sample.c)
target_link_libraries(sample pthread vsi-core vsi-api)

