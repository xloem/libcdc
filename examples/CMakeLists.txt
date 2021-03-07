# Includes
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR} )

# Targets
add_executable(find_all find_all.c)
add_executable(simple simple.c)
add_executable(serial_test serial_test.c)

# Linkage
target_link_libraries(find_all cdc)
target_link_libraries(simple cdc)
target_link_libraries(serial_test cdc)

# Source includes
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src)
