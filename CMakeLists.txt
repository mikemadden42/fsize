cmake_minimum_required(VERSION 3.14)

project(fsize CXX)

set (CMAKE_CXX_STANDARD 17)
if (NOT MSVC)
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")
endif()

add_executable(fsize fsize.cc)

enable_testing()
# The test uses symlinks and chmod, so it only runs on Unix.
if (UNIX)
  add_test(NAME fsize_tree
           COMMAND ${CMAKE_COMMAND}
                   -DFSIZE=$<TARGET_FILE:fsize>
                   -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fsize_test
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/fsize_test.cmake)
endif()
