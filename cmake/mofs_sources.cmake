# MOFS OS-independent source lists (core, format, buffer cache, POSIX API).
# Included by standalone CMake (src/core, src/posix) and Zephyr (zephyr/CMakeLists.txt)
# so both build paths share the same .c file set without duplicating paths.

if(NOT DEFINED MOFS_ROOT)
    set(MOFS_ROOT ${PROJECT_SOURCE_DIR})
endif()

# MOFS core sources
set(MOFS_CORE_SOURCES
    ${MOFS_ROOT}/src/core/modules/mofs_core.c
    ${MOFS_ROOT}/src/core/modules/mofs_block.c
    ${MOFS_ROOT}/src/core/modules/mofs_inode.c
    ${MOFS_ROOT}/src/core/modules/mofs_dir.c
    ${MOFS_ROOT}/src/core/modules/mofs_path.c
    ${MOFS_ROOT}/src/core/modules/mofs_perm.c
    ${MOFS_ROOT}/src/core/modules/mofs_file.c
)

## MOFS format sources
set(MOFS_FORMAT_SOURCES
    ${MOFS_ROOT}/src/core/modules/mofs_format.c
)

## MOFS buffer sources
set(MOFS_BUFFER_UNIFIED_SOURCES
    ${MOFS_ROOT}/src/core/modules/mofs_buffer.c
)

set(MOFS_BUFFER_SPLIT_SOURCES
    ${MOFS_ROOT}/src/core/modules/mofs_buffer_split.c
)

## MOFS POSIX API sources
set(MOFS_POSIX_API_SOURCES
    ${MOFS_ROOT}/src/posix/posix.c
)
