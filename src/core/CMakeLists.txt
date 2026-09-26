# include mofs_sources.cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/mofs_sources.cmake)

# Build formatter library (no mofs_block: mkfs must not pull full FS core / `ctx`)
add_library(${CORE_FORMAT_LIB_NAME} STATIC
    ${MOFS_FORMAT_SOURCES}
)
target_link_libraries(${CORE_FORMAT_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME} mofs_port)
target_include_directories(${CORE_FORMAT_LIB_NAME} PRIVATE ./include)

# Build MOFS core library
add_library(${CORE_LIB_NAME} STATIC
    ${MOFS_CORE_SOURCES}
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
        target_sources(${CORE_LIB_NAME} PRIVATE ${MOFS_BUFFER_SPLIT_SOURCES})
    else()
        target_compile_definitions(${CORE_LIB_NAME} PUBLIC MOFS_BCACHE_SPLIT=0)
        target_sources(${CORE_LIB_NAME} PRIVATE ${MOFS_BUFFER_UNIFIED_SOURCES})
    endif()
else()
    target_compile_definitions(${CORE_LIB_NAME} PUBLIC MOFS_BUFFER_CACHE_ENABLE=0)
    target_sources(${CORE_LIB_NAME} PRIVATE ${MOFS_BUFFER_UNIFIED_SOURCES})
endif()
