
set(FORMATTER_NAME mkfs.mofs)

# Build mkfs.mofs
add_executable(${FORMATTER_NAME} mkfs_mofs.c)

# Link libos_dep.a
add_dependencies(${FORMATTER_NAME} ${OS_DEP_LIB_NAME})
target_link_libraries(${FORMATTER_NAME} ${OS_DEP_LIB_NAME})
