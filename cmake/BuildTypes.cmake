# BuildTypes.cmake
#
# Normalizes CMAKE_BUILD_TYPE and defines per-build-type compiler and
# linker flags used by the symfind executable.

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
