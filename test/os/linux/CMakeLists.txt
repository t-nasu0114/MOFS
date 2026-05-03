# OS abstraction tests: errno conversion contracts.
mofs_add_cmocka_test(test_os_errno
    SOURCES test_os_errno.c
    LINK_LIBS ${OS_SERVICE_LIB_NAME}
)

# OS abstraction tests: caller user/group context handling.
mofs_add_cmocka_test(test_os_user
    SOURCES test_os_user.c
    LINK_LIBS ${OS_SERVICE_LIB_NAME}
)

# OS abstraction tests: device I/O wrappers with temporary file fixtures.
mofs_add_cmocka_test(test_os_devio
    SOURCES test_os_devio.c
    LINK_LIBS
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
)
