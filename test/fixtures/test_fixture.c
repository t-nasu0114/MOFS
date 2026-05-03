#include "test_fixture.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int mofs_test_create_temp_image(char *out_path, size_t out_path_len, size_t size_bytes)
{
    int  fd     = -1;
    int  ret    = -1;
    char tmpl[] = "/tmp/mofs_test_XXXXXX";

    if ((out_path == NULL) || (out_path_len == 0U)) {
        return -1;
    }

    fd = mkstemp(tmpl);
    if (fd < 0) {
        return -1;
    }

    if (size_bytes > 0U) {
        if (ftruncate(fd, (off_t)size_bytes) != 0) {
            close(fd);
            unlink(tmpl);
            return -1;
        }
    }

    if (close(fd) != 0) {
        unlink(tmpl);
        return -1;
    }

    if (strlen(tmpl) + 1U > out_path_len) {
        unlink(tmpl);
        return -1;
    }

    strcpy(out_path, tmpl);
    ret = 0;
    return ret;
}

int mofs_test_remove_file(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    if (unlink(path) != 0) {
        return -1;
    }

    return 0;
}
