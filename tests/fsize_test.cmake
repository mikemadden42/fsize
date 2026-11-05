# Builds a small directory tree, runs fsize in it, and checks the output.
# Usage: cmake -DFSIZE=<path to fsize> -DWORK_DIR=<scratch dir> -P fsize_test.cmake

set(tree "${WORK_DIR}/tree")

# A previous run may have left the locked directory unreadable.
if(EXISTS "${tree}")
  execute_process(COMMAND chmod -R u+rwx "${tree}")
  file(REMOVE_RECURSE "${tree}")
endif()

# a/: 6 + 4 = 10 bytes of regular files.
file(WRITE "${tree}/a/f" "hello\n")
file(WRITE "${tree}/a/sub/g" "abc\n")

# b/: only a symlink and an unreadable directory, so 0 bytes.
file(MAKE_DIRECTORY "${tree}/b/locked")
file(CREATE_LINK "../a/f" "${tree}/b/link" SYMBOLIC)
execute_process(COMMAND chmod 000 "${tree}/b/locked")

# A top-level symlink to a directory is not reported.
file(CREATE_LINK "a" "${tree}/dirlink" SYMBOLIC)

execute_process(
  COMMAND "${FSIZE}"
  WORKING_DIRECTORY "${tree}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)

execute_process(COMMAND chmod u+rwx "${tree}/b/locked")

message(STATUS "stdout:\n${output}")
message(STATUS "stderr:\n${error}")

if(NOT result EQUAL 0)
  message(FATAL_ERROR "fsize exited with ${result}")
endif()

# Output order is unspecified, so check each line on its own.
foreach(expected "./a: 10 bytes" "./b: 0 bytes")
  string(FIND "${output}" "${expected}\n" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "missing line: ${expected}")
  endif()
endforeach()

string(REGEX MATCHALL "\n" lines "${output}")
list(LENGTH lines line_count)
if(NOT line_count EQUAL 2)
  message(FATAL_ERROR "expected 2 lines of output, got ${line_count}")
endif()
