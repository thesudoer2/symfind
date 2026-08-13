Refactor the CMake project structure of this C++20 project into a modern, feature-based layout without changing its functionality.

## Goals

Reorganize the repository into a clean, maintainable structure similar to modern C++ projects (LLVM, fmt, spdlog, etc.), but **do not** split the project into multiple libraries. The final build should still produce a **single executable target**.

## Directory structure

Move all public headers into:

```text
include/
└── symfind/
    ├── config/
    ├── core/
    ├── database/
    ├── elf/
    ├── filesystem/
    ├── utils/
    └── ...
```

Move implementation files into:

```text
src/
├── main.cpp
├── config/
├── core/
├── database/
├── elf/
├── filesystem/
├── utils/
└── ...
```

Mirror the structure between `include/symfind` and `src` so each `.cpp` has its corresponding header in the matching directory.

When deciding where to place a file, organize by **responsibility**, not by file extension:

* **config/**: configuration loading, command-line parsing, JSON parsing.
* **core/**: symbol model, search engine, string comparison, trigram indexing, dictionary building, and other core algorithms.
* **database/**: database reading/writing, serialization, compression, database structures, and persistence.
* **elf/**: ELF parsing and symbol extraction.
* **filesystem/**: filesystem traversal, bind mount handling, directory/file wrappers, memory-mapped files, and filesystem utilities.
* **utils/**: reusable helper classes that are not tied to a specific subsystem (threading, logging, common utilities, error handling, timing, etc.).

## CMake organization

Create a `cmake/` directory containing reusable configuration modules, for example:

```text
cmake/
├── BuildTypes.cmake
├── Dependencies.cmake
├── CompilerOptions.cmake
└── Ccache.cmake
```

Move the existing logic from the root `CMakeLists.txt` into these files where appropriate.

Examples:

* `BuildTypes.cmake`

  * build type handling
  * optimization flags
  * linker flags

* `Dependencies.cmake`

  * `find_package(...)`
  * `find_library(...)`

* `Ccache.cmake`

  * ccache detection

The root `CMakeLists.txt` should remain small and primarily configure the project before calling `add_subdirectory(src)`.

## Source collection

Each directory under `src/` should have its own `CMakeLists.txt`.

Do **not** create a library target for each directory.

Instead, each subdirectory should append its implementation files to a shared source list using `PARENT_SCOPE`, for example:

```cmake
list(APPEND SYMFIND_SOURCES
    DatabaseBuilder.cpp
    DatabaseReader.cpp
)

set(SYMFIND_SOURCES
    ${SYMFIND_SOURCES}
    PARENT_SCOPE
)
```

The executable should be created only once in `src/CMakeLists.txt`.

Avoid `file(GLOB ...)`; list source files explicitly.

## Dependencies

Dependencies should remain centralized.

`Dependencies.cmake` should only locate packages and libraries.

It should **not** call `target_link_libraries()`.

The executable should link dependencies in one place after it is created.

Keep existing dependency usage:

* re2
* gperftools (tcmalloc)
* unordered_dense
* robin_hood
* ZSTD
* static libelf
* static libm

Do not remove or change existing functionality.

## Includes

Configure the include path so all project headers are included like:

```cpp
#include <symfind/utils/Threading.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/database/DatabaseReader.h>
```

There should never be relative includes such as:

```cpp
#include "../../../include/symfind/utils/Threading.h"
```

Use:

```cmake
target_include_directories(symfind PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
```

and update all project includes accordingly.

## Constraints

* Preserve all functionality.
* Preserve all compiler flags and linker flags.
* Preserve all dependency versions.
* Do not introduce unnecessary abstraction.
* Do not create per-module libraries.
* Do not change any application logic.
* Do not remove any existing features.
* Prefer small, readable CMake files over complex CMake functions or macros.
* Keep the build easy to understand and maintain.

## Deliverables

1. Reorganized directory tree.
2. Updated `CMakeLists.txt` files.
3. Updated include directives throughout the project.
4. Any required path fixes after moving files.
5. Ensure the project builds successfully with the new layout.
