
# TODO : Dispatch source file
set(OS_DEP_LIB_NAME os_dep)
set(OS_DEP_LIB_NAME ${OS_DEP_LIB_NAME} PARENT_SCOPE)

set(OS_DEP_SRC_LIST
    devio_ubuntu.c
    errno_ubuntu.c
)

# Build libdevio.a
add_library(${OS_DEP_LIB_NAME} STATIC ${OS_DEP_SRC_LIST})
