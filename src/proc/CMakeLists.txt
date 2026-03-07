
if(${PLATFORM} STREQUAL "zephyr")
    # TBD
elseif(${PLATFORM} STREQUAL "linux")
    add_subdirectory(linux/fuse)
    add_subdirectory(linux/mkfs)
else()
    # TBD
endif()
