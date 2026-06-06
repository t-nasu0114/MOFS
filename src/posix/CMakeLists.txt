set(POSIX_API_LAYER_SRC_LIST
    posix.c
)

add_library(${POSIX_API_LAYER_LIB_NAME} STATIC ${POSIX_API_LAYER_SRC_LIST})

target_link_libraries(${POSIX_API_LAYER_LIB_NAME} PUBLIC ${CORE_LIB_NAME} mofs_port)

target_include_directories(${POSIX_API_LAYER_LIB_NAME} PRIVATE
    ../core/include
)
