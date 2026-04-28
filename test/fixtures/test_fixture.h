#ifndef MOFS_TEST_FIXTURE_H
#define MOFS_TEST_FIXTURE_H

#include <stddef.h>

int mofs_test_create_temp_image(char *out_path, size_t out_path_len, size_t size_bytes);
int mofs_test_remove_file(const char *path);

#endif /* MOFS_TEST_FIXTURE_H */
