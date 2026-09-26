cmake_minimum_required(VERSION 3.20.0)

# Apply the RAM disk overlay before Zephyr is loaded.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ramdisk.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mofs_vfs_main)

target_sources(app PRIVATE mofs_vfs_main.c)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
