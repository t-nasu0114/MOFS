cmake_minimum_required(VERSION 3.10)
project(MyProject)
set(CMAKE_C_STANDARD 99)

if(ZEPHYR_BASE)
    set(PLATFORM "zephyr")
else()
    set(PLATFORM "linux")
endif()

# source list
set(SRC_LIST
    ./src/main.c
)

# build executable
set(EXE_NAME main)
add_executable(${EXE_NAME} ${SRC_LIST})

# original headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# build OS dependent part
add_subdirectory(src/os)

# build core part
add_subdirectory(src/core)

# build process
add_subdirectory(src/proc)
