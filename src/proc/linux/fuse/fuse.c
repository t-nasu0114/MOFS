/* fuse.c
 * FUSE implementation for Linux
 */

#include "fuse_ops.h"
#include <fcntl.h>
#include <fuse.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(void)
{
    printf("Usage: %s DEVICE_FILE MOUNT_POINT\n", SELF_NAME);
    printf("Options:\n");
    printf("  -h, --help         Display this help message\n");
}

int main(int argc, char *argv[])
{
    const char *mount_point  = NULL;
    const char *devfile_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
        if (argv[i][0] == '-') {
            printf("Unknown option: %s\n", argv[i]);
            print_usage();
            return 1;
        }
        if (devfile_path == NULL) {
            devfile_path = argv[i];
        } else if (mount_point == NULL) {
            mount_point = argv[i];
        } else {
            printf("Too many arguments: %s\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    if (devfile_path == NULL || mount_point == NULL) {
        printf("DEVICE_FILE and MOUNT_POINT are required\n");
        print_usage();
        return 1;
    }

    /* FUSE main */
    mofs_fuse_ctx_t fuse_ctx;
#if 1 /* Normal*/
    int   fuse_argc    = 2;
    char *fuse_argv[2] = {argv[0], (char *)mount_point};
#else /* Debug*/
    int   fuse_argc    = 4;
    char *fuse_argv[4] = {argv[0], "-f", "-d", (char *)mount_point};
#endif
    int ret               = 0;
    fuse_ctx.devfile_path = (char *)devfile_path;

    ret = fuse_main(fuse_argc, fuse_argv, &op, &fuse_ctx);
    return ret;
}
