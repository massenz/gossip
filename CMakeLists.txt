# This file (c) 2016 AlertAvert.com.  All rights reserved.

project(brick)
cmake_minimum_required(VERSION 3.4)


# Include common cmake utilities:
include(${COMMON_UTILS_DIR}/commons.cmake OPTIONAL RESULT_VARIABLE COMMONS_FILE)

### Setting up common configuration variables

# Version number
set(RELEASE_MAJOR 0)
set(RELEASE_MINOR 3)
set(RELEASE_PATCH 0)

IF (${COMMONS_FILE} STREQUAL NOTFOUND)
    message(WARNING "Not using CMake utilities")
    set(BUILD_ID 0)
ELSE (${COMMONS_FILE} STREQUAL NOTFOUND)
    get_build_id(BUILD_ID)
ENDIF (${COMMONS_FILE} STREQUAL NOTFOUND)

set(RELEASE_STRING "${RELEASE_MAJOR}.${RELEASE_MINOR}.${RELEASE_PATCH}-${BUILD_ID}")
message(STATUS "Building Release: ${RELEASE_STRING}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(TESTS_DIR ${PROJECT_SOURCE_DIR}/tests)
set(PROJECT_BINARY_DIR ${PROJECT_SOURCE_DIR}/build)


# Configure a header file to pass settings into source code
configure_file(
    ${SOURCE_DIR}/config.h.in
    ${PROJECT_BINARY_DIR}/config.h
)

include_directories(${SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}
        ${LOCAL_INSTALL_DIR}/include
)

set(SOURCE_FILES
        ${SOURCE_DIR}/brick.cpp
        ${SOURCE_DIR}/Bucket.cpp
        ${SOURCE_DIR}/View.cpp
        ${SOURCE_DIR}/MerkleNode.cpp)

add_executable(brick ${SOURCE_FILES} ${SOURCE_DIR}/main.cpp)
target_link_libraries(brick ssl crypto glog)

### Unit tests build is defined in the tests/ folder
add_subdirectory(${TESTS_DIR})