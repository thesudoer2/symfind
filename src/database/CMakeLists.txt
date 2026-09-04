target_sources(symfind_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Database.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseBuilderUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ZSTDCompressor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ZSTDDictionary.cpp
)
