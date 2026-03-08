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

typedef struct
{
    int   dev_fd;       /* Device file descriptor */
    char *devfile_path; /* Device file path used by MOFS backend (opened on DEVICE_FILE). */
} mofs_ctx_t;

static struct fuse_operations op = {
    .getattr = mofs_getattr,
    .readdir = mofs_readdir,
    .read    = mofs_read,
};

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
    mofs_ctx_t ctx;
    int        fuse_argc    = 2;
    char      *fuse_argv[2] = {argv[0], (char *)mount_point};
    int        ret          = 0;
    ctx.devfile_path        = (char *)malloc(sizeof(char) * PATH_MAX);
    ctx.dev_fd              = open(devfile_path, O_RDWR | O_EXCL);

    if (ctx.devfile_path == NULL) {
        printf("Fail to allocate memory of strings\n");
        return 1;
    } else if (ctx.dev_fd < 0) {
        printf("Fail to open device file\n");
        return 1;
    }

    strncpy(ctx.devfile_path, devfile_path, PATH_MAX);
    ret = fuse_main(fuse_argc, fuse_argv, &op, &ctx);
    free(ctx.devfile_path);
    close(ctx.dev_fd);
    return ret;
}
