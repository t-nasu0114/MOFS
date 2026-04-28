# Core lifecycle tests (init/fini/stat observable behavior).
mofs_add_cmocka_test(test_core_lifecycle
    SOURCES test_core_lifecycle.c
    LINK_LIBS
        ${CORE_LIB_NAME}
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
)

# Format and initialization integration tests.
mofs_add_cmocka_test(test_core_format
    SOURCES test_core_format.c
    LINK_LIBS
        ${CORE_FORMAT_LIB_NAME}
        ${CORE_LIB_NAME}
        ${OS_SERVICE_LIB_NAME}
        mofs_test_fixture
)
