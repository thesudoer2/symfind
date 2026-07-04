cmake_minimum_required(VERSION 3.22)
project(symfind LANGUAGES CXX)

set(PROGRAM_NAME ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Check build type
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

set(SYMFIND_BUILD_FLAGS)
set(SYMFIND_LINK_FLAGS)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    list(APPEND SYMFIND_BUILD_FLAGS -O0 -g3 -fno-omit-frame-pointer)
    list(APPEND SYMFIND_LINK_FLAGS -Wl,-O0)
elseif(CMAKE_BUILD_TYPE STREQUAL "RELEASE")
    list(APPEND SYMFIND_BUILD_FLAGS -O3 -DNDEBUG)
    list(APPEND SYMFIND_LINK_FLAGS -Wl,-O3)
elseif(CMAKE_BUILD_TYPE STREQUAL "RELWITHDEBINFO")
    list(APPEND SYMFIND_BUILD_FLAGS -O2 -DNDEBUG -fno-omit-frame-pointer)
    list(APPEND SYMFIND_LINK_FLAGS -Wl,-O2)
else()
    message(WARNING "Unknown build type: ${CMAKE_BUILD_TYPE}")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build flags: ${SYMFIND_BUILD_FLAGS}")
message(STATUS "Link flags: ${SYMFIND_LINK_FLAGS}")

# Find ccache program
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
else()
    message(WARNING "Could not find `ccache' program!")
endif()

# Find dependency packages
find_package(re2 REQUIRED)
find_package(gperftools REQUIRED COMPONENTS tcmalloc_minimal)
find_package(unordered_dense CONFIG REQUIRED)
find_package(robin_hood CONFIG REQUIRED)
find_package(ZSTD REQUIRED)

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


file(GLOB SOURCES "*.cpp")

add_executable(${PROGRAM_NAME} ${SOURCES})

target_compile_options(${PROGRAM_NAME} PRIVATE
    -Wall
    -Wextra
    -Werror
    ${SYMFIND_BUILD_FLAGS}
)

target_link_options(${PROGRAM_NAME} PRIVATE
    -Wl,--as-needed
    -Wl,--no-copy-dt-needed-entries
    ${SYMFIND_LINK_FLAGS}
)

target_link_libraries(${PROGRAM_NAME} PRIVATE
    -static
    ${LIBELF_STATIC_LIBRARY}
    ${LIBM_STATIC_LIBRARY}
    ${zstd_LIBRARIES}
    re2::re2
    gperftools::gperftools
    unordered_dense::unordered_dense
    robin_hood::robin_hood
)
