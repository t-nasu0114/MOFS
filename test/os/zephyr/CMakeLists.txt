cmake_minimum_required(VERSION 3.20.0)

# Apply the RAM disk overlay before Zephyr is loaded.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ramdisk.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mofs_fs_test)

set(FS_COMMON $ENV{ZEPHYR_BASE}/tests/subsys/fs/common)

target_sources(app PRIVATE
    src/main.c
    src/testfs_mofs.c
    ${FS_COMMON}/test_fs_util.c
    ${FS_COMMON}/test_fs_basic.c
    ${FS_COMMON}/test_fs_dirops.c
    ${FS_COMMON}/test_fs_open_flags.c
)

target_include_directories(app PRIVATE
    ${FS_COMMON}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/os/zephyr/tools/vfs
)
