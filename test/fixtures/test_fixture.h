#ifndef MOFS_TEST_FIXTURE_H
#define MOFS_TEST_FIXTURE_H

#include <mofs_types.h>

int mofs_test_create_temp_image(char *out_path, mofs_size_t out_path_len, mofs_size_t size_bytes);
int mofs_test_remove_file(const char *path);

#endif /* MOFS_TEST_FIXTURE_H */
