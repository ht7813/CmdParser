cmake_minimum_required(VERSION 3.14)
project(CmdParserTest)

set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_cmdparser test_cmdparser.cpp)
target_link_libraries(test_cmdparser ${GTEST_LIBRARIES} pthread)