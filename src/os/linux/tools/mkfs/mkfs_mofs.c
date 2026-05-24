
#include <mofs_format.h>
#include <mofs_util.h>
#include <stdlib.h>
#include <unistd.h>

static void print_usage(const char *prog_name)
{
    MOFS_INF("Usage: %s [OPTIONS] DEVICE_FILE\n", prog_name);
    MOFS_INF("Options:\n");
    MOFS_INF("  -s, --size <NUM>   Specify file system size in blocks (default: auto)\n");
    MOFS_INF("  -b, --block <NUM> Logical block size in bytes (This value must be a (512 * (2 ^ n)) and between 512 "
             "and 65536)\n");
    MOFS_INF("  -h, --help         Display this help message\n");
}

int main(int argc, char *argv[])
{
    int   opt;
    int   fs_size        = -1;
    char *device_file    = NULL;
    int   blk_size_param = -1;

    while ((opt = getopt(argc, argv, "s:b:h")) != -1) {
        switch (opt) {
        case 's':
            fs_size = atoi(optarg);
            break;
        case 'b':
            blk_size_param = atoi(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            /* nothing */
            break;
        }
    }

    /* Check device file name */
    if (optind < argc) {
        device_file = argv[optind];
    } else {
        MOFS_ERR("Device file is required.\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Format device */
    if (mofs_format(device_file, fs_size, blk_size_param) != 0) {
        MOFS_ERR("Format error\n");
        return 1;
    }

    return 0;
}
