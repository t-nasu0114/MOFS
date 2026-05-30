# Build formatter library (no mofs_block: mkfs must not pull full FS core / `ctx`)
add_library(${CORE_FORMAT_LIB_NAME} STATIC
    modules/mofs_format.c
)
target_link_libraries(${CORE_FORMAT_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME})
target_include_directories(${CORE_FORMAT_LIB_NAME} PRIVATE
    ${PLATFORM}/include
    ./include
)

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
target_link_libraries(${CORE_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME})
target_include_directories(${CORE_LIB_NAME} PRIVATE ./include)