# Includes
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR})

# Version information
set(SNAPSHOT_VERSION "unknown")
execute_process(COMMAND git describe
                OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
                RESULT_VARIABLE GIT_DESCRIBE_RESULT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )
if(${GIT_DESCRIBE_RESULT} STREQUAL 0)
    set(SNAPSHOT_VERSION ${GIT_DESCRIBE_OUTPUT})
endif()
message(STATUS "Detected git snapshot version: ${SNAPSHOT_VERSION}")

configure_file(cdc_version_i.h.in "${CMAKE_CURRENT_BINARY_DIR}/cdc_version_i.h" @ONLY)

# Targets
set(c_sources   ${CMAKE_CURRENT_SOURCE_DIR}/cdc.c CACHE INTERNAL "List of c sources")
set(c_headers   ${CMAKE_CURRENT_SOURCE_DIR}/cdc.h CACHE INTERNAL "List of c headers")

add_library(cdc SHARED ${c_sources})

set_target_properties(cdc PROPERTIES VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.0 SOVERSION 2)
# Prevent clobbering each other during the build
set_target_properties(cdc PROPERTIES CLEAN_DIRECT_OUTPUT 1)


# Dependencies
target_link_libraries(cdc ${LIBUSB_LIBRARIES})

install( TARGETS cdc
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib${LIB_SUFFIX}
         ARCHIVE DESTINATION lib${LIB_SUFFIX}
       )

if( STATICLIBS )
    add_library(cdc-static STATIC ${c_sources})
    target_link_libraries(cdc-static ${LIBUSB_LIBRARIES})
    set_target_properties(cdc-static PROPERTIES OUTPUT_NAME "cdc")
    set_target_properties(cdc-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    install( TARGETS cdc-static
             ARCHIVE DESTINATION lib${LIB_SUFFIX}
             COMPONENT staticlibs
           )
endif()

install( FILES ${c_headers}
         DESTINATION include/${PROJECT_NAME}
         COMPONENT headers
       )
