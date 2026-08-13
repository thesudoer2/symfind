# symfind

**symfind** is a fast, C++20 command-line tool for locating symbols (function and object names) inside ELF binaries and shared libraries across a filesystem. It scans directories, parses ELF symbol tables, and lets you search for a symbol name using exact, regex, or fuzzy matching — either on the fly or against a prebuilt, compressed symbol database for near-instant lookups.

> Think of it as a `locate`/`mlocate` for ELF symbols: build an index once, then query it in milliseconds.

## Features

- **Fast symbol search** across large filesystem trees, backed by a trigram index and a compact on-disk database.
- **Three search modes**: exact string match, regular expressions (via [re2](https://github.com/google/re2)), and fuzzy matching (via [RapidFuzz](https://github.com/rapidfuzz/rapidfuzz-cpp)).
- **Three running modes**:
  - `build_db` — scan the filesystem and build/update the symbol database.
  - `read_db` — query the prebuilt database (default mode).
  - `free_run` — scan and search on the fly, without touching the database.
- **ELF symbol extraction** for defined and runtime/imported symbols, with visibility filtering (`defined`, `runtime`, or `both`).
- **Bind-mount aware filesystem scanning**, with configurable pruning of filesystems, paths, and names to skip.
- **Compressed, serialized database** (via [Zstandard](https://github.com/facebook/zstd)) with a trained dictionary for efficient storage and fast decompression.
- **High-performance hash maps** (via [ankerl::unordered_dense](https://github.com/martinus/unordered_dense) and [robin_hood](https://github.com/martinus/robin-hood-hashing)) for the in-memory symbol index.
- **JSON-based configuration** with sensible defaults, so it works out of the box.
- **Statically linked executable** for easy, dependency-free deployment.

## Dependencies

symfind requires a C++20 compiler and the following libraries:

| Dependency | Purpose |
|---|---|
| [re2](https://github.com/google/re2) | Regex-based symbol matching |
| [gperftools](https://github.com/gperftools/gperftools) (tcmalloc) | High-performance memory allocation |
| [unordered_dense](https://github.com/martinus/unordered_dense) | Fast hash maps/sets |
| [robin_hood](https://github.com/martinus/robin-hood-hashing) | Fast hash maps/sets |
| [Zstandard (ZSTD)](https://github.com/facebook/zstd) | Database compression |
| libelf (static) | ELF file parsing |
| libm (static) | Math routines |
| [nlohmann/json](https://github.com/nlohmann/json) | Configuration file parsing |
| [tl::expected](https://github.com/TartanLlama/expected) | Error handling |
| [RapidFuzz (C++)](https://github.com/rapidfuzz/rapidfuzz-cpp) | Fuzzy string matching |

Build tooling:

- CMake ≥ 3.22
- A C++20-capable compiler (GCC or Clang)
- [ccache](https://ccache.dev/) (optional, auto-detected, speeds up rebuilds)

## Build Instructions

### 1. Install dependencies

On a Debian/Ubuntu-based system:

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libre2-dev libgoogle-perftools-dev libzstd-dev libelf-dev
```

`unordered_dense`, `robin_hood`, `nlohmann/json`, `tl::expected`, and `rapidfuzz-cpp` are header-only libraries. Install them system-wide (e.g. under `/usr/local`) or make them discoverable via `CMAKE_PREFIX_PATH` / your package manager of choice (vcpkg, Conan, etc.).

### 2. Configure and build

```bash
git clone https://github.com/thesudoer2/symfind.git
cd symfind

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Available build types: `Debug`, `Release`, `RelWithDebInfo`.

### 3. Install (optional)

```bash
sudo cp build/symfind /usr/local/bin/
```

The resulting `symfind` binary is statically linked against its dependencies for easy distribution.

## Configuration

symfind reads a JSON configuration file, by default from:

```
/etc/symfind/symfind.conf
```

Example configuration:

```json
{
  "database_path": "/var/lib/symfind/symfind.db",
  "database_scan_path": "/",
  "prune_bind_mounts": "true",
  "debug_pruning": "false",
  "prunefs": "proc sysfs tmpfs devtmpfs",
  "prunepaths": "/proc /sys /dev /tmp",
  "prunenames": ".git node_modules"
}
```

| Key | Description |
|---|---|
| `database_path` | Path to the symbol database file. |
| `database_scan_path` | Root path to scan when building the database. |
| `prune_bind_mounts` | Skip bind-mounted directories during scanning (`true`/`false`). |
| `debug_pruning` | Log filesystem pruning decisions for debugging (`true`/`false`). |
| `prunefs` | Space-separated list of filesystem types to skip. |
| `prunepaths` | Space-separated list of paths to skip. |
| `prunenames` | Space-separated list of file/directory names to skip. |

## Usage

```
symfind [options] <symbol>
```

### Options

| Option | Description |
|---|---|
| `-r, --running-method <build_db\|read_db\|free_run>` | Choose the program's running behavior. Default: `read_db`. |
| `-r, --regex` | Shortcut for `--search-type regex`. |
| `-f, --fuzz` | Shortcut for `--search-type fuzzy`. |
| `--search-type <default\|regex\|fuzzy>` | How symbol matching is performed. Default: `default` (exact match). |
| `--symbol-visibility <defined\|runtime\|both>` | Which symbols to show. Default: `defined`. |
| `--root <path>` | Root directory for symbol search. |
| `-v, --verbose` | Be verbose and show more logs. |
| `-h, --help` | Show the help message. |

> **Note:** `-r` is used both as the shortcut for `--running-method` and `--regex`; check the program's `--help` output for the current behavior in your build.

## Example Command Lines

Build (or rebuild) the symbol database:

```bash
symfind --running-method build_db
```

Look up an exact symbol name against the database (default mode):

```bash
symfind malloc
```

Search using a regular expression:

```bash
symfind --search-type regex "std::.*vector"
```

Fuzzy-search for a misspelled symbol:

```bash
symfind --search-type fuzzy pushbak
```

Only show runtime/imported symbols:

```bash
symfind --symbol-visibility runtime malloc
```

Search a specific directory tree without using the database:

```bash
symfind --running-method free_run --root /usr/lib64 printf
```

## Project Structure

The project follows a feature-based layout: public headers live under `include/symfind/`, mirrored by implementation files under `src/`. The build produces a single executable target — there are no per-module libraries.

```text
symfind/
├── CMakeLists.txt              # Root CMake entry point (small, delegates to src/)
├── cmake/                      # Reusable CMake modules
│   ├── BuildTypes.cmake        # Build-type flags (Debug/Release/RelWithDebInfo)
│   ├── Ccache.cmake            # ccache detection
│   ├── CompilerOptions.cmake   # C++ standard & warning flags
│   └── Dependencies.cmake      # find_package()/find_library() calls only
├── include/
│   └── symfind/
│       ├── config/             # Configuration loading, CLI parsing, JSON parsing
│       ├── core/                # Symbol model, search engine, string comparison, trigram indexing
│       ├── database/            # Database read/write, serialization, compression
│       ├── elf/                  # ELF parsing and symbol extraction
│       ├── filesystem/           # Filesystem traversal, bind mounts, file/dir wrappers
│       └── utils/                 # Threading, logging, generic helpers
├── src/
│   ├── main.cpp
│   ├── config/
│   ├── core/
│   ├── database/
│   ├── elf/
│   ├── filesystem/
│   └── utils/
├── benchmark/                  # Standalone micro-benchmarks (not part of the main build)
├── LICENSE
└── README.md
```

All internal headers are included using the `symfind/<category>/Header.h` form, e.g.:

```cpp
#include <symfind/utils/Threading.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/database/DatabaseReader.h>
```

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository and create a feature branch.
2. Follow the existing code style (see `.clang-format` and `.clang-tidy`).
3. Keep changes focused — avoid unrelated refactors in the same pull request.
4. Make sure the project builds cleanly and existing functionality still works.
5. Open a pull request describing the motivation and the change.

If you find a bug or have a feature request, please open an issue with as much detail as possible (steps to reproduce, expected vs. actual behavior, environment info).

## License

This project is licensed under the [MIT License](LICENSE). See the `LICENSE` file for the full text.
