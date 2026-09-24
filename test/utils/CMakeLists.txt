list(APPEND SYMFIND_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ThreadingComputeFileIdChunks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ThreadingGetProperThreadCount.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ThreadingLaunchThreadsOverFiles.cpp
)

set(SYMFIND_TEST_SOURCES
    ${SYMFIND_TEST_SOURCES}
    PARENT_SCOPE
)
