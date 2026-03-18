
# Setting for FUSE
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

# Executable name
set(FUSE_EXE_NAME mofs)

add_executable(${FUSE_EXE_NAME} fuse.c fuse_ops.c)

add_dependencies(${FUSE_EXE_NAME} ${OS_DEP_LIB_NAME} ${CORE_LIB_NAME})
target_link_libraries(${FUSE_EXE_NAME}
    PRIVATE ${OS_DEP_LIB_NAME}
    PRIVATE ${CORE_LIB_NAME}
)
target_compile_definitions(${FUSE_EXE_NAME} PRIVATE SELF_NAME=\"${FUSE_EXE_NAME}\")

# Setting for FUSE
target_compile_definitions(${FUSE_EXE_NAME} PRIVATE FUSE_USE_VERSION=31)
target_link_libraries(${FUSE_EXE_NAME} PRIVATE ${FUSE_LIBRARIES})
target_include_directories(${FUSE_EXE_NAME} PRIVATE ${FUSE_INCLUDE_DIRS})
target_compile_options(${FUSE_EXE_NAME} PRIVATE ${FUSE_CFLAGS_OTHER})

# copy binaries
add_custom_command(TARGET ${FUSE_EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory /home/t-nasu/work/test
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${FUSE_EXE_NAME}>
            /home/t-nasu/work/test/
)