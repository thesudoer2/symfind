list(APPEND SYMFIND_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseWriterTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseBuilderUtilsTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseBuilderTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseReaderTest.cpp
)

set(SYMFIND_TEST_SOURCES
    ${SYMFIND_TEST_SOURCES}
    PARENT_SCOPE
)