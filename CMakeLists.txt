cmake_minimum_required(VERSION 3.10)
project(MyProject)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug for single-config generators.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug Release RelWithDebInfo MinSizeRel
)

# Make Debug builds easy to inspect with gdb.
string(APPEND CMAKE_C_FLAGS_DEBUG " -O0 -g3")

if(ZEPHYR_BASE)
    set(PLATFORM "zephyr")
else()
    set(PLATFORM "linux")
endif()

# original headers
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src/port/include)

# -----------------------------------------------------------------------------
# Feature toggles
# -----------------------------------------------------------------------------

# Block buffer cache. Overrides the default in src/core/include/mofs_config.h.
option(MOFS_ENABLE_BUFFER_CACHE "Enable block buffer cache" ON)

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

# build port (HAL contract) part
add_subdirectory(src/port)

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
