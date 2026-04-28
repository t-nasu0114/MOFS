# MOFS test directory

This directory contains `cmocka` based tests grouped by layer.

## Layout

- `fixtures/`: shared test utilities for temporary test files.
- `os/linux/`: OS abstraction layer tests (`os_service`).
- `posix/`: public POSIX-like API tests (`mofs_open`, `mofs_read`, etc.).
- `core/`: core and format layer tests (`mofs_init_core`, `mofs_format`, etc.).

## Build and run

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests are always built as part of the normal build.
To check registered tests without running them:

```sh
ctest --test-dir build -N
```

## Implementation phases

1. Baseline: enable test build and pass `test_os_errno`.
2. Expand P2 tests (`test_os_devio`, `test_os_user`).
3. Implement P0 POSIX tests with image fixtures.
4. Implement P1 lifecycle/format tests with fixture setup.
