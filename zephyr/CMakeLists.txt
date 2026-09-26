if(CONFIG_MOFS)
  # MOFS_ROOT is the root directory of the mofs module
  set(MOFS_ROOT ${ZEPHYR_MOFS_MODULE_DIR})

  # 1. include common source list
  include(${ZEPHYR_MOFS_MODULE_DIR}/cmake/mofs_sources.cmake)

  # 2. register library (do not create multiple STATIC libraries)
  zephyr_library_named(mofs)

  # 3. enumerate sources (core + posix + format + buffer + os/zephyr stubs)
  zephyr_library_sources(
    ${MOFS_CORE_SOURCES}
    ${MOFS_FORMAT_SOURCES}
    ${MOFS_POSIX_API_SOURCES}
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_devio.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_errno.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_log.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_mem.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_posix_errno.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_str.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_sync.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_time.c
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/service/os_user.c
  )
  if(CONFIG_MOFS_BUFFER_CACHE)
    if(CONFIG_MOFS_BCACHE_SPLIT)
      zephyr_library_sources(${MOFS_BUFFER_SPLIT_SOURCES})
    else()
      zephyr_library_sources(${MOFS_BUFFER_UNIFIED_SOURCES})
    endif()
  else()
    zephyr_library_sources(${MOFS_BUFFER_UNIFIED_SOURCES})
  endif()

  if(CONFIG_MOFS_FS)
    zephyr_library_sources(
      ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/tools/vfs/mofs_vfs.c
    )
  endif()

  # 4. include path
  # public API -> visible from entire application
  zephyr_include_directories(${ZEPHYR_MOFS_MODULE_DIR}/include)
  # internal headers -> only mofs library
  zephyr_library_include_directories(
    ${ZEPHYR_MOFS_MODULE_DIR}/src/core/include
    ${ZEPHYR_MOFS_MODULE_DIR}/src/port/include
    ${ZEPHYR_MOFS_MODULE_DIR}/src/os/zephyr/include
  )

  # 5. Kconfig -> mofs_config.h macros
  if(CONFIG_MOFS_BUFFER_CACHE)
    zephyr_library_compile_definitions(MOFS_BUFFER_CACHE_ENABLE=1)
  else()
    zephyr_library_compile_definitions(MOFS_BUFFER_CACHE_ENABLE=0)
  endif()
  # BCACHE_SPLIT is also similar
endif()