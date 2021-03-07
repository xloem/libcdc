# Includes
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR} )

# Targets
add_executable(simple simple.c)

# Linkage
target_link_libraries(simple cdc)

# Source includes
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src)
