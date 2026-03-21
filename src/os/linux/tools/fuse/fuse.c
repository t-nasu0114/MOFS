/* fuse.c
 * FUSE implementation for Linux
 */

#include "fuse_ops.h"
#include <fcntl.h>
#include <fuse.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Print command usage and supported options.
 *
 * Function behavior:
 * - Prints the required command format.
 * - Prints supported help options.
 *
 * @param[in] none No input parameters.
 * @param[out] none No output parameters.
 * @return No return value.
 */
static void print_usage(void)
{
    printf("Usage: %s DEVICE_FILE MOUNT_POINT\n", SELF_NAME);
    printf("Options:\n");
    printf("  -h, --help         Display this help message\n");
}

/**
 * @brief Parse CLI arguments and start the FUSE main loop.
 *
 * Function behavior:
 * - Parses `DEVICE_FILE` and `MOUNT_POINT` from command-line arguments.
 * - Handles help and unknown option cases.
 * - Builds FUSE arguments and invokes `fuse_main()`.
 *
 * @param[in] argc Number of command-line arguments.
 * @param[in] argv Array of command-line argument strings.
 * @param[out] none No output parameters.
 * @return 0 when help is printed successfully.
 * @return 1 on invalid or missing command-line arguments.
 * @return Value returned by `fuse_main()` when FUSE is started.
 */
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
#if 0 /* Normal*/
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
