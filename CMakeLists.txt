cmake_minimum_required(VERSION 3.22)
project(symfind LANGUAGES CXX)

set(PROGRAM_NAME ${PROJECT_NAME})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CompilerOptions)
include(BuildTypes)
include(Ccache)
include(Dependencies)

add_subdirectory(src)
