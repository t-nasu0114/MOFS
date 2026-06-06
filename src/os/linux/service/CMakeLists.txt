find_package(Threads REQUIRED)

# OS service source list
set(OS_SERVICE_SRC_LIST
    os_devio.c
    os_errno.c
    os_mem.c
    os_posix_errno.c
    os_str.c
    os_time.c
    os_user.c
)

# Build libos_service.a
add_library(${OS_SERVICE_LIB_NAME} STATIC ${OS_SERVICE_SRC_LIST})

target_include_directories(${OS_SERVICE_LIB_NAME} PUBLIC ../include)
target_link_libraries(${OS_SERVICE_LIB_NAME} PUBLIC Threads::Threads)
