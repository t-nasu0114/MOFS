/* fuse.c
 * FUSE implementation for Linux
 */

#include "fuse_ops.h"
#include <fuse.h>
#include <string.h>
#include <unistd.h>
#include <mofs_log.h>

static struct fuse_operations op = {
    .getattr = mofs_getattr,
    .readdir = mofs_readdir,
    .read    = mofs_read,
};

static void print_usage(void)
{
    MOFS_INF("Usage: %s DEVICE_FILE MOUNT_POINT\n", SELF_NAME);
    MOFS_INF("Options:\n");
    MOFS_INF("  -h, --help         Display this help message\n");
}

int main(int argc, char *argv[])
{
    const char *device_file = NULL;
    const char *mount_point = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
        if (argv[i][0] == '-') {
            MOFS_ERR("Unknown option: %s\n", argv[i]);
            print_usage();
            return 1;
        }
        if (device_file == NULL) {
            device_file = argv[i];
        } else if (mount_point == NULL) {
            mount_point = argv[i];
        } else {
            MOFS_ERR("Too many arguments: %s\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    if (device_file == NULL || mount_point == NULL) {
        MOFS_ERR("DEVICE_FILE and MOUNT_POINT are required\n");
        print_usage();
        return 1;
    }

    /* TODO: mount device_file at mount_point via FUSE */
    (void)device_file;
    (void)mount_point;
    fuse_main(argc, argv, &op, NULL);

    return 0;
}
