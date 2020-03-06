# This file (c) 2016-2020 AlertAvert.com.  All rights reserved.

project(gossip)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fPIC")
cmake_policy(SET CMP0068 NEW)

# Version number
set(RELEASE_MAJOR 1)
set(RELEASE_MINOR 0)
set(RELEASE_PATCH 2)


# Include common cmake utilities:
include(${COMMON_UTILS_DIR}/commons.cmake OPTIONAL RESULT_VARIABLE COMMONS_FILE)
IF (${COMMONS_FILE} STREQUAL NOTFOUND)
    message(WARNING "Not using CMake utilities")
    set(BUILD_ID 0)
ELSE (${COMMONS_FILE} STREQUAL NOTFOUND)
    get_build_id(BUILD_ID)
ENDIF (${COMMONS_FILE} STREQUAL NOTFOUND)

##
#
# Installation directory; this is both used to access
# required libraries (microhttpd, distutils) as well as
# to install the compiled artifacts.
#
# The INSTALL_DIR env variable must be set, unless -DINSTALL_DIR is used.
if(NOT DEFINED INSTALL_DIR)
    if(DEFINED ENV{INSTALL_DIR})
        set(INSTALL_DIR $ENV{INSTALL_DIR})
    else()
        message(FATAL_ERROR
                "INSTALL_DIR should be defined as an environment variable, or via -D notation")
    endif()
endif()

if(NOT DEFINED LIBMICROHTTPD_INSTALL)
    set(LIBMICROHTTPD_INSTALL ${INSTALL_DIR}/libmicrohttpd)
endif()

# Conan Packaging support
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(RELEASE_STRING "${RELEASE_MAJOR}.${RELEASE_MINOR}.${RELEASE_PATCH}-${BUILD_ID}")
message(STATUS "Building Release: ${RELEASE_STRING}")


set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(TESTS_DIR ${PROJECT_SOURCE_DIR}/tests)
set(PROTO_DIR ${PROJECT_SOURCE_DIR}/proto)
set(PROJECT_BINARY_DIR ${PROJECT_SOURCE_DIR}/build)

# Configure a header file to pass settings into source code.
configure_file(
    ${SOURCE_DIR}/config.h.in
    ${PROJECT_BINARY_DIR}/version.h
)

# Building Protocol Buffers.
#
find_package(Protobuf REQUIRED)

IF(${PROTOBUF_FOUND})
    PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${PROTO_DIR}/swim.proto)
    message(STATUS "When generated, Protocol Buffer files will be in:\n"
        "\tHeaders: ${PROTO_HDRS}\n\tSources: ${PROTO_SRCS}\n"
        "protoc: ${PROTOBUF_PROTOC_EXECUTABLE}\n"
        "protoc lib: ${PROTOBUF_PROTOC_LIBRARY}\n"
        "protobuf lib: ${PROTOBUF_LIBRARY}"
)
ELSE(${PROTOBUF_FOUND})
    message(FATAL_ERROR "Could not find Google Protocol Buffers libraries")
ENDIF(${PROTOBUF_FOUND})

# libmicrohttpd HTTP Server (https://www.gnu.org/software/libmicrohttpd)
set(LIBMICROHTTPD_INSTALL ${INSTALL_DIR}/libmicrohttpd)

include_directories(
        ${INCLUDE_DIR}
        ${PROJECT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
        ${PROTO_HDRS}
        ${LIBMICROHTTPD_INSTALL}/include
        ${INSTALL_DIR}/include
)

link_directories(
        ${LIBMICROHTTPD_INSTALL}/lib
        ${INSTALL_DIR}/lib
)

set(SWIM_SOURCES
        ${SOURCE_DIR}/swim/SwimClient.cpp
        ${SOURCE_DIR}/swim/SwimServer.cpp
        ${SOURCE_DIR}/swim/GossipFailureDetector.cpp
)

set(SWIM_LIBS
        distutils
        glog
        microhttpd
        protobuf
        pthread
        zmq
)

##
# libgossip.so
#
# Shared (dynamic) library for the SWIM Gossip Protocol.
#
add_library(gossip SHARED
        ${SWIM_SOURCES}
        ${PROTO_SRCS}
)
target_link_libraries(gossip ${SWIM_LIBS})


if (DEFINED INSTALL_DIR)
    message(STATUS "Installing shared libraries to ${INSTALL_DIR}/lib "
                   "and include files to ${INSTALL_DIR}/include/${PROJECT_NAME}")
    install(TARGETS gossip DESTINATION ${INSTALL_DIR}/lib)

    # Generated configuration file, containing version and build number.
    install(FILES ${PROJECT_BINARY_DIR}/version.h DESTINATION
            "${INSTALL_DIR}/include/${PROJECT_NAME}")

    # Install Library headers.
    install(DIRECTORY ${INCLUDE_DIR}/ DESTINATION ${INSTALL_DIR}/include/${PROJECT_NAME}
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

    # Copy Proto headers to include folder.
    install(FILES ${PROTO_HDRS} DESTINATION ${INSTALL_DIR}/include/${PROJECT_NAME}/proto)

else ()
    message(FATAL_ERROR "INSTALL_DIR is not defined, files will not be installed."
                        " Use -DINSTALL_DIR=/path/to/install to enable")
endif ()

# Unit tests build is defined in the tests/ folder
add_subdirectory(${TESTS_DIR})

#########
# Examples - demo execs/programs to show usage of the libraries/utilities.
#########

set(EXAMPLES_DIR ${SOURCE_DIR}/examples)

# Examples will use the latest build for the shared libraries
link_directories(
        ${CMAKE_BINARY_DIR}/lib
        ${INSTALL_DIR}/lib
)


##
# SWIM Server
#
# Demo server to either listen to incoming pings, or send.
# Basic usage of the `SwimServer` class.
#
add_executable(swim_server_demo
        ${EXAMPLES_DIR}/server_demo.cpp
)
target_link_libraries(swim_server_demo gossip)

##
# Gossip Server
#
# Full implementation of the `GossipFailureDetector`.
#
add_executable(gossip_detector_example
        ${EXAMPLES_DIR}/gossip_example.cpp
)
target_link_libraries(gossip_detector_example gossip apiserver)
