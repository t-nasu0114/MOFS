cmake_minimum_required(VERSION 3.10)

# Detect cmocka from package targets first, then fallback to manual lookup.
find_package(cmocka QUIET)

if(TARGET cmocka::cmocka)
    set(MOFS_CMOCKA_TARGET cmocka::cmocka)
elseif(TARGET cmocka)
    set(MOFS_CMOCKA_TARGET cmocka)
else()
    find_path(CMOCKA_INCLUDE_DIR cmocka.h)
    find_library(CMOCKA_LIBRARY cmocka)
    if((NOT CMOCKA_INCLUDE_DIR) OR (NOT CMOCKA_LIBRARY))
        message(FATAL_ERROR "cmocka not found. Install libcmocka-dev or provide CMAKE_PREFIX_PATH.")
    endif()

    add_library(mofs_cmocka UNKNOWN IMPORTED)
    set_target_properties(mofs_cmocka PROPERTIES
        IMPORTED_LOCATION "${CMOCKA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CMOCKA_INCLUDE_DIR}"
    )
    set(MOFS_CMOCKA_TARGET mofs_cmocka)
endif()

function(mofs_add_cmocka_test target_name)
    # Helper to define one cmocka executable and register it to CTest.
    cmake_parse_arguments(MOFS_TEST "" "" "SOURCES;LINK_LIBS" ${ARGN})
    add_executable(${target_name} ${MOFS_TEST_SOURCES})
    target_link_libraries(${target_name}
        PRIVATE
            ${MOFS_CMOCKA_TARGET}
            ${MOFS_TEST_LINK_LIBS}
    )
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# Build shared fixtures first, then layer-specific test suites.
add_subdirectory(fixtures)
add_subdirectory(os/linux)
add_subdirectory(posix)
add_subdirectory(core)
