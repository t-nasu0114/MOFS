# Build formatter library (no mofs_block: mkfs must not pull full FS core / `ctx`)
add_library(${CORE_FORMAT_LIB_NAME} STATIC
    modules/mofs_format.c
)
target_link_libraries(${CORE_FORMAT_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME} mofs_port)
target_include_directories(${CORE_FORMAT_LIB_NAME} PRIVATE ./include)

# Build MOFS core library
add_library(${CORE_LIB_NAME} STATIC
    modules/mofs_core.c
    modules/mofs_block.c
    modules/mofs_inode.c
    modules/mofs_dir.c
    modules/mofs_path.c
    modules/mofs_perm.c
    modules/mofs_file.c
)
target_link_libraries(${CORE_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME} mofs_port)
target_include_directories(${CORE_LIB_NAME} PRIVATE ./include)

# Buffer cache toggle and implementation selection.
# MOFS_BUFFER_CACHE_ENABLE overrides the default in mofs_config.h.
# MOFS_BCACHE_SPLIT=1 selects the split-pool implementation; 0 selects unified.
if(MOFS_ENABLE_BUFFER_CACHE)
    target_compile_definitions(${CORE_LIB_NAME} PUBLIC MOFS_BUFFER_CACHE_ENABLE=1)
    if(MOFS_BCACHE_IMPL STREQUAL "split")
        target_compile_definitions(${CORE_LIB_NAME} PUBLIC MOFS_BCACHE_SPLIT=1)
        target_sources(${CORE_LIB_NAME} PRIVATE modules/mofs_buffer_split.c)
    else()
        target_compile_definitions(${CORE_LIB_NAME} PUBLIC MOFS_BCACHE_SPLIT=0)
        target_sources(${CORE_LIB_NAME} PRIVATE modules/mofs_buffer.c)
    endif()
else()
    target_compile_definitions(${CORE_LIB_NAME} PUBLIC MOFS_BUFFER_CACHE_ENABLE=0)
    target_sources(${CORE_LIB_NAME} PRIVATE modules/mofs_buffer.c)
endif()
