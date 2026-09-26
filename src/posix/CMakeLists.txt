# include mofs_sources.cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/mofs_sources.cmake)

set(POSIX_API_LAYER_SRC_LIST
    ${MOFS_POSIX_API_SOURCES}
)

add_library(${POSIX_API_LAYER_LIB_NAME} STATIC ${POSIX_API_LAYER_SRC_LIST})

target_link_libraries(${POSIX_API_LAYER_LIB_NAME} PUBLIC ${CORE_LIB_NAME} mofs_port)

target_include_directories(${POSIX_API_LAYER_LIB_NAME} PRIVATE
    ../core/include
)
