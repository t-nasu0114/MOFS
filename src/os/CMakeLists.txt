
# Library name can be referred by proc
set(OS_DEP_LIB_NAME os_dep)
set(OS_DEP_LIB_NAME ${OS_DEP_LIB_NAME} PARENT_SCOPE)

if(${PLATFORM} STREQUAL "zephyr")
    # TBD
elseif(${PLATFORM} STREQUAL "linux")
    set(OS_DEP_SRC_LIST
        linux/devio_linux.c
        linux/errno_linux.c
    )
else()
    # TBD
endif()

# Build libdevio.a
add_library(${OS_DEP_LIB_NAME} STATIC ${OS_DEP_SRC_LIST})
