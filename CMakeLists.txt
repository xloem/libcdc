# Project
project(libcdc C)
set(MAJOR_VERSION 0)
set(MINOR_VERSION 1)
set(PACKAGE libcdc)
set(VERSION_STRING ${MAJOR_VERSION}.${MINOR_VERSION})
set(VERSION ${VERSION_STRING})

# CMake
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE    RelWithDebInfo)
endif("${CMAKE_BUILD_TYPE}" STREQUAL "")
set(CMAKE_COLOR_MAKEFILE ON)
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

add_definitions(-Wall)

option( STATICLIBS "Build static libraries" ON )
option( DOCUMENTATION "Generate API Documentation with Doxygen" OFF )
option( EXAMPLES "Build example programs" ON )

# Debug build
message("-- Build type: ${CMAKE_BUILD_TYPE}")
if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
    add_definitions(-DDEBUG)
endif(${CMAKE_BUILD_TYPE} STREQUAL Debug)

# find libusb
find_package( PkgConfig )
pkg_check_modules( LIBUSB libusb-1.0 )
include_directories( ${LIBUSB_INCLUDE_DIRS} )

# guess LIB_SUFFIX, don't take debian multiarch into account
if( NOT DEFINED LIB_SUFFIX )
    if( CMAKE_SYSTEM_NAME MATCHES "Linux"
            AND NOT CMAKE_CROSSCOMPILING
            AND NOT EXISTS "/etc/debian_version"
            AND NOT EXISTS "/etc/arch-release" )
        if( "${CMAKE_SIZEOF_VOID_P}" EQUAL "8" )
            set( LIB_SUFFIX 64 )
        endif()
    endif()
endif()

# "make dist" target
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${VERSION_STRING})
add_custom_target(dist
    COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
            | bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

if( DOCUMENTATION )
    find_package( Doxygen REQUIRED)

    # Copy doxy.config.in
    set(top_srcdir ${CMAKE_SOURCE_DIR})
    configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile)
    configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.xml.in ${CMAKE_BINARY_DIR}/Doxyfile.xml)

    # Run doxygen
    add_custom_command(
     OUTPUT ${CMAKE_BINARY_DIR}/doc/html/index.html
     COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
     COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
     DEPENDS ${c_headers};${c_source};${cpp_source};${cpp_headers}
    )

    add_custom_target(docs ALL DEPENDS ${CMAKE_BINARY_DIR}/doc/html/index.html)
endif()

add_subdirectory(src)
if( EXAMPLES )
    add_subdirectory(examples)
endif()

# PkgConfig
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX}/bin)
set(includedir  ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME})

if(${UNIX})
    set(libdir      ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
endif(${UNIX})
if(${WIN32})
    set(libdir      ${CMAKE_INSTALL_PREFIX}/bin)
endif(${WIN32})

configure_file(${CMAKE_SOURCE_DIR}/libcdc.pc.in ${CMAKE_BINARY_DIR}/libcdc.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/libcdc.pc DESTINATION lib${LIB_SUFFIX}/pkgconfig)

if(UNIX OR MINGW)
    configure_file( libcdc-config.in ${CMAKE_CURRENT_BINARY_DIR}/libcdc-config @ONLY)
    install( PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/libcdc-config DESTINATION bin)
endif()

message(STATUS "Summary of build options:

    Build static libs: ${STATICLIBS}
    Build examples: ${EXAMPLES}
    Build API documentation: ${DOCUMENTATION}
")
