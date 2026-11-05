# Builds a small directory tree, runs fsize on it, and checks the output.
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

# A loose top-level file counts toward the total only: 10 + 0 + 8 = 18.
file(WRITE "${tree}/top.txt" "1234567\n")

# Top-level symlinks are neither reported nor counted.
file(CREATE_LINK "a" "${tree}/dirlink" SYMBOLIC)
file(CREATE_LINK "top.txt" "${tree}/filelink" SYMBOLIC)

# Runs fsize with the given arguments from working directory `dir` and sets
# fsize_result and fsize_output in the caller's scope.
function(run_fsize dir)
  execute_process(
    COMMAND "${FSIZE}" ${ARGN}
    WORKING_DIRECTORY "${dir}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  message(STATUS "fsize ${ARGN} (in ${dir}) -> ${result}\n"
                 "stdout:\n${output}stderr:\n${error}")
  set(fsize_result "${result}" PARENT_SCOPE)
  set(fsize_output "${output}" PARENT_SCOPE)
endfunction()

# Checks a successful run. Output order is unspecified, so each expected
# line is checked on its own, along with the total number of lines.
function(expect_lines)
  if(NOT fsize_result EQUAL 0)
    message(FATAL_ERROR "fsize exited with ${fsize_result}")
  endif()
  foreach(expected IN LISTS ARGN)
    string(FIND "${fsize_output}" "${expected}\n" pos)
    if(pos EQUAL -1)
      message(FATAL_ERROR "missing line: ${expected}")
    endif()
  endforeach()
  string(REGEX MATCHALL "\n" lines "${fsize_output}")
  list(LENGTH lines line_count)
  list(LENGTH ARGN expected_count)
  if(NOT line_count EQUAL expected_count)
    message(FATAL_ERROR
            "expected ${expected_count} lines of output, got ${line_count}")
  endif()
endfunction()

# Checks a successful run whose output must match exactly, in order. The
# arguments are joined into the expected output.
function(expect_output)
  string(CONCAT expected ${ARGV})
  if(NOT fsize_result EQUAL 0)
    message(FATAL_ERROR "fsize exited with ${fsize_result}")
  endif()
  if(NOT fsize_output STREQUAL expected)
    message(FATAL_ERROR "expected output:\n${expected}got:\n${fsize_output}")
  endif()
endfunction()

function(expect_failure)
  if(fsize_result EQUAL 0)
    message(FATAL_ERROR "expected fsize to fail")
  endif()
endfunction()

# No argument: scans the current directory. Largest folder first.
run_fsize("${tree}")
expect_output("./a: 10 bytes\n./b: 0 bytes\ntotal: 18 bytes\n")

# Directory argument.
run_fsize("${WORK_DIR}" "${tree}")
expect_lines("${tree}/a: 10 bytes" "${tree}/b: 0 bytes" "total: 18 bytes")

# Missing path, a file instead of a directory, and too many arguments.
run_fsize("${WORK_DIR}" "${tree}/missing")
expect_failure()
run_fsize("${WORK_DIR}" "${tree}/top.txt")
expect_failure()
run_fsize("${WORK_DIR}" "${tree}" "${tree}")
expect_failure()

# Unknown option.
run_fsize("${WORK_DIR}" -x)
expect_failure()

execute_process(COMMAND chmod u+rwx "${tree}/b/locked")

# Many top-level folders, more than the worker pool has threads. Each holds
# 2 bytes directly and 3 bytes two levels down, so 5 bytes per folder.
set(many "${WORK_DIR}/many")
file(REMOVE_RECURSE "${many}")
set(expected "")
foreach(i RANGE 1 300)
  file(WRITE "${many}/d${i}/x" "x\n")
  file(WRITE "${many}/d${i}/y/z/w" "yz\n")
  list(APPEND expected "./d${i}: 5 bytes")
endforeach()
list(APPEND expected "total: 1500 bytes")
run_fsize("${many}")
expect_lines(${expected})

# Sizes that land in different units, a tie, and an empty folder.
#   big:     1536 KiB  -> 1.5M
#   mid:       20 KiB  -> 20.0K
#   another:  512 B    -> 512B  (ties with small, so sorted by name)
#   small:    512 B    -> 512B
#   zero:       0 B    -> 0B
# Total 1594368 bytes, about 1.52 MiB -> 1.5M.
set(units "${WORK_DIR}/units")
file(REMOVE_RECURSE "${units}")
function(make_file path kib)
  get_filename_component(dir "${path}" DIRECTORY)
  file(MAKE_DIRECTORY "${dir}")
  execute_process(COMMAND dd if=/dev/zero "of=${path}" bs=1024 count=${kib}
                  ERROR_QUIET)
endfunction()
make_file("${units}/big/f" 1536)
make_file("${units}/mid/f" 20)
string(REPEAT "x" 512 half_kib)
file(WRITE "${units}/another/f" "${half_kib}")
file(WRITE "${units}/small/f" "${half_kib}")
file(MAKE_DIRECTORY "${units}/zero")

run_fsize("${units}")
expect_output("./big: 1572864 bytes\n./mid: 20480 bytes\n./another: 512 bytes\n"
              "./small: 512 bytes\n./zero: 0 bytes\ntotal: 1594368 bytes\n")

# -h works before or after the directory.
foreach(args "-h" "-h;." ".;-h")
  run_fsize("${units}" ${args})
  expect_output("./big: 1.5M\n./mid: 20.0K\n./another: 512B\n./small: 512B\n"
                "./zero: 0B\ntotal: 1.5M\n")
endforeach()
