cmake_minimum_required(VERSION 3.10)
project(MyProject)
set(CMAKE_C_STANDARD 99)

if(ZEPHYR_BASE)
    set(PLATFORM "zephyr")
else()
    set(PLATFORM "linux")
endif()

# Setting for FUSE
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

# source list
set(SRC_LIST
    ./src/main.c
)

# build executable
set(EXE_NAME main)
add_executable(${EXE_NAME} ${SRC_LIST})

# original headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Setting for FUSE
target_compile_definitions(${EXE_NAME} PRIVATE FUSE_USE_VERSION=31)
target_link_libraries(${EXE_NAME} PRIVATE ${FUSE_LIBRARIES})
target_include_directories(${EXE_NAME} PRIVATE ${FUSE_INCLUDE_DIRS})
target_compile_options(${EXE_NAME} PRIVATE ${FUSE_CFLAGS_OTHER})

# build OS dependent part
add_subdirectory(src/os)

# build core part
add_subdirectory(src/core)

# build process
add_subdirectory(src/proc)
