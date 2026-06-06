# POSIX-like file operation tests (open/read/close behavior).
mofs_add_cmocka_test(test_posix_file_ops
    SOURCES test_posix_file_ops.c
    LINK_LIBS
        ${POSIX_API_LAYER_LIB_NAME}
        ${CORE_LIB_NAME}
        ${CORE_FORMAT_LIB_NAME}
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
)

# POSIX-like directory operation tests (opendir/readdir/closedir behavior).
mofs_add_cmocka_test(test_posix_dir_ops
    SOURCES test_posix_dir_ops.c
    LINK_LIBS
        ${POSIX_API_LAYER_LIB_NAME}
        ${CORE_LIB_NAME}
        ${CORE_FORMAT_LIB_NAME}
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
)

# POSIX-like miscellaneous behavior tests (e.g. stat on mounted image).
mofs_add_cmocka_test(test_posix_misc
    SOURCES test_posix_misc.c
    LINK_LIBS
        ${POSIX_API_LAYER_LIB_NAME}
        ${CORE_LIB_NAME}
        ${CORE_FORMAT_LIB_NAME}
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
)

find_package(Threads REQUIRED)

# Per-thread mofs_errno isolation tests.
mofs_add_cmocka_test(test_posix_errno_tls
    SOURCES test_posix_errno_tls.c
    LINK_LIBS
        ${POSIX_API_LAYER_LIB_NAME}
        ${CORE_LIB_NAME}
        ${CORE_FORMAT_LIB_NAME}
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
        Threads::Threads
)
