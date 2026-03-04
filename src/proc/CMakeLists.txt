
if(${PLATFORM} STREQUAL "zephyr")
    # TBD
elseif(${PLATFORM} STREQUAL "linux")
    add_subdirectory(linux/mkfs_mofs)
else()
    # TBD
endif()
