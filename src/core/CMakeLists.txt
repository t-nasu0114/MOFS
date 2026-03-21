# Build formatter library
add_library(${CORE_FORMAT_LIB_NAME} STATIC
    modules/mofs_format.c
    modules/mofs_core_util.c
)
target_link_libraries(${CORE_FORMAT_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME})
target_include_directories(${CORE_FORMAT_LIB_NAME} PUBLIC 
    ${PLATFORM}/include
    ./include
)

# Build MOFS core library
add_library(${CORE_LIB_NAME} STATIC
    modules/mofs_core.c
    modules/mofs_core_util.c
    modules/mofs_inode.c
)
target_link_libraries(${CORE_LIB_NAME} PUBLIC ${OS_SERVICE_LIB_NAME})
target_include_directories(${CORE_LIB_NAME} PUBLIC ./include)