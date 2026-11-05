# TODO

## Bugs

- [x] **Any filesystem error aborts the whole run** (`fsize.cc:11`). `recursive_directory_iterator` throws on an unreadable directory (or a file deleted mid-walk); the exception reaches `future.get()` uncaught and calls `std::terminate`, so every remaining result is lost.
  ```
  libc++abi: terminating due to uncaught exception of type ...filesystem_error:
    attempting recursion into "./b/locked": Permission denied
  ```
  Fix: use `fs::directory_options::skip_permission_denied` with the `std::error_code` overloads (iterator, `is_regular_file`, `file_size`), and wrap `future.get()` in a try/catch.
- [x] **Files behind symlinks are counted** (`fsize.cc:18-19`). `entry.is_regular_file()` and `entry.file_size()` follow symlinks, so a link to a large file elsewhere adds its full size, and linked files inside the tree are counted twice. Skip links with `entry.is_symlink()` or check `symlink_status()`.
- [x] **Top-level symlinked directories are followed** (`fsize.cc:39`). `entry.is_directory()` is true for a symlink to a directory, so the tool walks the target, which may be outside the tree or a duplicate.
- [ ] **Hard links are counted more than once, and sizes are apparent sizes, not disk usage.** Both differ from `du`. Decide which `fsize` should report and document it.

## Design gaps

- [ ] Count files directly in `.` and print a total.
- [ ] Take the path from `argv[1]` instead of always using `.`.
- [ ] Limit the number of threads. `std::launch::async` starts one thread per top-level directory with no cap. Use a worker pool or cap at `std::thread::hardware_concurrency()`.
- [ ] Balance the load. One huge subdirectory keeps a single thread busy while the rest sit idle.
- [ ] Sort output by size and print human-readable units (K, M, G).

## Build and CI

- [x] Fix the `ctest` step: there's no `enable_testing()` or `add_test`, so CMake 4.x `ctest` prints "No test configuration file found!" and exits 1. Add a test or remove the step.
- [x] Replace `set(CMAKE_CXX_FLAGS ...)` with `target_compile_options(fsize PRIVATE -Wall -Wextra -pedantic)` so user-supplied flags aren't overwritten.
- [x] Add `find_package(Threads REQUIRED)` and `target_link_libraries(fsize PRIVATE Threads::Threads)`. `std::async` can fail on older glibc/GCC without it.
- [x] Add `set(CMAKE_CXX_STANDARD_REQUIRED ON)`.
- [x] Update `actions/checkout@v3`, add a macOS/Windows build matrix, and remove the template comments from the workflow.

## Minor

- [x] Add `#include <cstdint>` and `#include <utility>`. The build works now only because other headers happen to include them.
- [ ] Expand the README: usage, sample output, and how symlinks and hard links are handled.
- [x] Add a CTest case that builds a temporary tree (nested directories, a symlink, an unreadable directory).
