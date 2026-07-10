# CompilerOptions.cmake
#
# Global language standard settings and the common warning flags applied
# to the symfind executable.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SYMFIND_WARNING_FLAGS
    -Wall
    -Wextra
    -Werror
)
