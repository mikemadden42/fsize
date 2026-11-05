cmake_minimum_required(VERSION 3.14)

project(fsize CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(fsize fsize.cc)
target_link_libraries(fsize PRIVATE Threads::Threads)
if (NOT MSVC)
  target_compile_options(fsize PRIVATE -Wall -Wextra -pedantic)
endif()

install(TARGETS fsize)

enable_testing()
# The test uses symlinks and chmod, so it only runs on Unix.
if (UNIX)
  add_test(NAME fsize_tree
           COMMAND ${CMAKE_COMMAND}
                   -DFSIZE=$<TARGET_FILE:fsize>
                   -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fsize_test
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/fsize_test.cmake)
endif()
