# Ccache.cmake
#
# Detects ccache and, if available, wires it up as the compiler launcher
# for both C and C++ to speed up rebuilds.

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
else()
    message(WARNING "Could not find `ccache' program!")
endif()
