set(DEPENDENCIES
    -static
    ${LIBELF_STATIC_LIBRARY}
    ${LIBM_STATIC_LIBRARY}
    ${zstd_LIBRARIES}
    re2::re2
    gperftools::gperftools
    unordered_dense::unordered_dense
    robin_hood::robin_hood
    rapidfuzz::rapidfuzz
    tl::expected
    nlohmann_json::nlohmann_json
)

add_library(symfind_objects OBJECT)

target_include_directories(symfind_objects PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(symfind_objects PRIVATE
    ${DEPENDENCIES}
)

target_compile_options(symfind_objects PRIVATE
    ${SYMFIND_WARNING_FLAGS}
    ${SYMFIND_BUILD_FLAGS}
)

target_link_options(symfind_objects PRIVATE
    -Wl,--as-needed
    -Wl,--no-copy-dt-needed-entries
    ${SYMFIND_LINK_FLAGS}
)

add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(database)
add_subdirectory(elf)
add_subdirectory(filesystem)
add_subdirectory(utils)

add_executable(${PROGRAM_NAME}
    Main.cpp
    $<TARGET_OBJECTS:symfind_objects>
)

target_include_directories(${PROGRAM_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(${PROGRAM_NAME} PRIVATE
    ${SYMFIND_WARNING_FLAGS}
    ${SYMFIND_BUILD_FLAGS}
)

target_link_options(${PROGRAM_NAME} PRIVATE
    -Wl,--as-needed
    -Wl,--no-copy-dt-needed-entries
    ${SYMFIND_LINK_FLAGS}
)

target_link_libraries(${PROGRAM_NAME} PRIVATE
    ${DEPENDENCIES}
)
