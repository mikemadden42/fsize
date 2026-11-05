# fsize

Calculate folder sizes in parallel.

`fsize` reports the size of each folder directly inside a directory, plus a
total. The walk is split into one job per folder and shared by a pool of
worker threads, one per CPU core, so a single large folder doesn't leave the
other threads idle.

## Building

Requires CMake 3.14 or newer and a C++17 compiler.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/fsize`.

## Usage

```sh
fsize [-h] [directory]
```

With no directory, `fsize` scans the current directory. Folders are listed
largest first, with ties in name order, and the total comes last.

```console
$ fsize
./photos: 3145728 bytes
./music: 524288 bytes
./docs: 20480 bytes
total: 3691520 bytes
```

`-h` prints sizes in powers of 1024 (`B`, `K`, `M`, `G`, `T`, `P`, `E`) with
one decimal place. It can go before or after the directory.

```console
$ fsize -h
./photos: 3.0M
./music: 512.0K
./docs: 20.0K
total: 3.5M
```

Folder names are printed under the path you pass, so `fsize /usr` prints
lines like `/usr/bin: …`.

## What is counted

- **Sizes are apparent sizes**, the sum of each file's length in bytes. This
  is what Finder shows as "Size" and what `ls -l` shows per file. It is not
  disk usage, so it will usually differ from `du`: compressed, sparse, and
  cloned files can take less space on disk than their length, and small files
  can take more.
- **Only regular files are counted.** Directories themselves, devices, sockets,
  and so on add nothing.
- **Symlinks are skipped**, both inside folders and at the top level. A
  symlink to a file adds nothing, and a symlink to a folder is not listed or
  followed.
- **Hard links are counted once per link.** A file with two hard links in the
  scanned tree is counted twice. `du` counts it once.
- **The total includes loose files.** Regular files directly in the scanned
  directory are added to the total but do not get a line of their own.
- **Unreadable folders are skipped silently.** A folder `fsize` has no
  permission to read contributes 0 bytes, so a total can be lower than the
  real size when run without enough permissions.

## Exit status

`fsize` exits with 0 on success and 1 if:

- an unknown option or more than one directory is given (it prints
  `usage: fsize [-h] [directory]`),
- the path doesn't exist or isn't a directory,
- the directory can't be listed.

If a folder's walk stops partway through (for example, because something
was deleted while `fsize` was running), `fsize` prints a warning to stderr
that its size may be incomplete, still reports the other folders and the
total, and exits with 0.

## Testing

```sh
cd build
ctest --output-on-failure
```

The test builds a small directory tree with nested files, symlinks, and an
unreadable folder, then checks the output and the error cases. It needs
symlinks and `chmod`, so it only runs on Unix-like systems.

## License

MIT. See [LICENSE](LICENSE).
