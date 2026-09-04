# Dependencies.cmake
#
# Locates all third-party packages and libraries required by symfind.
# This file only calls find_package()/find_library(); linking is done
# once, in src/CMakeLists.txt, after the executable target is created.

# Find dependency packages
find_package(re2 REQUIRED)
find_package(gperftools REQUIRED COMPONENTS tcmalloc_minimal)
find_package(unordered_dense CONFIG REQUIRED)
find_package(robin_hood CONFIG REQUIRED)
find_package(ZSTD REQUIRED)
find_package(rapidfuzz REQUIRED)
find_package(tl-expected REQUIRED)
find_package(nlohmann_json REQUIRED)

# Find libelf library
find_library(LIBELF_STATIC_LIBRARY
    NAMES libelf.a
)
if (NOT LIBELF_STATIC_LIBRARY)
    message(FATAL_ERROR "Couldn't find `libelf.a' library!")
endif()

# Find libm library
find_library(LIBM_STATIC_LIBRARY
    NAMES libm.a
)
if (NOT LIBM_STATIC_LIBRARY)
    message(FATAL_ERROR "Couldn't find `libm.a' library!")
endif()

# Print found targets
# get_property(allTargets DIRECTORY PROPERTY IMPORTED_TARGETS)
# message(STATUS "Imported targets:")
# foreach(t ${allTargets})
#     message(STATUS "Found ${t}")
# endforeach()
