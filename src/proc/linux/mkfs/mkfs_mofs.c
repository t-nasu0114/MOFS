
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_log.h>
#include <mofs_struct.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

/* TODO: This extern will be deleted */
extern int mofs_format(const char *device_file, int fs_size, int blk_size);

static void print_usage(const char *prog_name)
{
    MOFS_INF("Usage: %s [OPTIONS] DEVICE_FILE\n", prog_name);
    MOFS_INF("Options:\n");
    MOFS_INF("  -s, --size <NUM>   Specify file system size in blocks (default: auto)\n");
    MOFS_INF("  -h, --help         Display this help message\n");
}

int main(int argc, char *argv[])
{
    int   fd;
    int   opt;
    int   fs_size  = -1;
    int   blk_size = -1;
    char *device_file;

    while ((opt = getopt(argc, argv, "s:b:h")) != -1) {
        switch (opt) {
        case 's':
            fs_size = atoi(optarg);
            break;
        case 'b':
            blk_size = atoi(optarg);
            if (blk_size != MOFS_BLK_SIZE) {
                MOFS_ERR("Only %d block size is supported.\n", MOFS_BLK_SIZE);
                return 1;
            }
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
    if (mofs_format(device_file, fs_size, blk_size) != 0) {
        MOFS_ERR("Format error\n");
        return 1;
    }

    return 0;
}
