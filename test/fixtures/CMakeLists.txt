# Shared fixture helpers used by multiple test executables.
add_library(mofs_test_fixture STATIC
    test_fixture.c
)

# Export fixture headers to dependent test targets.
target_include_directories(mofs_test_fixture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src/core/include
)
