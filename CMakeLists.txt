cmake_minimum_required(VERSION 3.10)
project(MyProject)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ZEPHYR_BASE)
    set(PLATFORM "zephyr")
else()
    set(PLATFORM "linux")
endif()

# original headers
include_directories(${PROJECT_SOURCE_DIR}/include)
# OS dependent headers
include_directories(src/os/${PLATFORM}/include)

# -----------------------------------------------------------------------------
# Library setting parts
# -----------------------------------------------------------------------------

# setting OS service part
set(OS_SERVICE_LIB_NAME os_service)

# setting core part
set(CORE_LIB_NAME mofs_core)
set(CORE_FORMAT_LIB_NAME mofs_format)

# setting POSIX API layer part
set(POSIX_API_LAYER_LIB_NAME posix_api)

# -----------------------------------------------------------------------------
# build parts
# -----------------------------------------------------------------------------

# build OS service part
add_subdirectory(src/os/${PLATFORM}/service)

# build core part
add_subdirectory(src/core)

# build POSIX API layer part
add_subdirectory(src/posix)

# build tools part
add_subdirectory(src/os/${PLATFORM}/tools)

# build test
enable_testing()
add_subdirectory(test)
