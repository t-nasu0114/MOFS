
# Build formatter
set(FORMATTER_LIB_NAME mofs_format)
set(FORMATTER_LIB_NAME ${FORMATTER_LIB_NAME} PARENT_SCOPE)

add_library(${FORMATTER_LIB_NAME} STATIC
    mofs_format.c
    mofs_util.c
)
target_link_libraries(${FORMATTER_LIB_NAME} PUBLIC ${OS_DEP_LIB_NAME})

# Build core library
set(CORE_LIB_NAME mofs_core)
set(CORE_LIB_NAME ${CORE_LIB_NAME} PARENT_SCOPE)

add_library(${CORE_LIB_NAME} STATIC
    mofs_core.c
    mofs_util.c
)
target_link_libraries(${CORE_LIB_NAME} PUBLIC ${OS_DEP_LIB_NAME})