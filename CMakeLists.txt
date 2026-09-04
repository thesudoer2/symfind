cmake_minimum_required(VERSION 3.22)
project(symfind LANGUAGES CXX)

set(PROGRAM_NAME ${PROJECT_NAME})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(EXPORT_COMPILE_COMMANDS ON)

include(CompilerOptions)
include(BuildTypes)
include(Ccache)
include(Dependencies)

add_subdirectory(src)

if (ENABLE_TESTS)
    add_subdirectory(test)
endif()
