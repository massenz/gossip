# This file (c) 2016-2017 AlertAvert.com.  All rights reserved.

project(gossip_test)
enable_testing()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fPIC")


# Conan Packaging support
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()


find_library(GTEST gtest ${INSTALL_DIR}/lib)
if (${GTEST} STREQUAL GTEST-NOTFOUND)
    message(FATAL_ERROR "Could not locate a valid Google Test library installed.")
endif (${GTEST} STREQUAL GTEST-NOTFOUND)

set(UNIT_TESTS
        ${TESTS_DIR}/test_gossip.cpp
        ${TESTS_DIR}/test_swim_server.cpp
)

# Add the build directory to the library search path
link_directories(${CMAKE_BINARY_DIR})

set(INTEGRATION_TESTS
        ${TESTS_DIR}/test_integration.cpp
)

# This is necessary as the .pb.cc and .pb.h files will not (yet) be present
# when we run cmake, and thus it will fail complaining it can't find the files.
# See http://stackoverflow.com/questions/19343018/cmake-and-findprotobuf as to
# why this is only necessary in this file (but not the parent).
#
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES
                            GENERATED TRUE)

add_executable(unit_tests
        ${UNIT_TESTS}
        tests.h
        all_tests.cpp
)
target_link_libraries(unit_tests
        ${GTEST}
        protobuf
        gossip
        distutils
)

add_executable(integration_tests
        ${INTEGRATION_TESTS}
        tests.h
        all_tests.cpp
)
target_link_libraries(integration_tests
        ${GTEST}
        gossip
        http_parser
        uv
)

add_test(unit_tests ${CMAKE_CURRENT_BINARY_DIR}/unit_tests)
add_test(integration_tests ${CMAKE_CURRENT_BINARY_DIR}/integration_tests)
