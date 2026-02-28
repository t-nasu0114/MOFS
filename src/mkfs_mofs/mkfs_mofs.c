
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_log.h>
#include <mofs_struct.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(const char *prog_name)
{
    MOFS_INF("Usage: %s [OPTIONS] DEVICE_FILE\n", prog_name);
    MOFS_INF("Options:\n");
    MOFS_INF("  -s, --size <NUM>   Specify file system size in blocks (default: auto)\n");
    MOFS_INF("  -h, --help         Display this help message\n");
}

static int clear_blocks(int fd, uint32_t block_num)
{
    int   ret;
    char *buf[MOFS_BLK_SIZE];
    off_t offset;

    memset(buf, 0, MOFS_BLK_SIZE);

    offset = (off_t)block_num * MOFS_BLK_SIZE;

    if ((dev_lseek(fd, offset, MOFS_SEEK_CUR) < 0)) {
        MOFS_ERR("Seek error at block %d", block_num);
        ret = get_errno();
    }

    if (dev_write(fd, buf, MOFS_BLK_SIZE) != MOFS_BLK_SIZE) {
        MOFS_ERR("Write error at block %d", block_num);
        ret = get_errno();
    }
    return ret;
}

static int mofs_format(const char *device_file, int fs_size, int blk_size)
{
    int                ret = 0;
    int                fd;
    unsigned long long dev_size;
    (void)blk_size;

    fd = dev_open(device_file, MOFS_IO_OPEN_FLAG_RDWR);
    if (fd < 0) {
        MOFS_ERR("Open %s error\n", device_file);
        ret = get_errno();
        goto out1;
    }

    /* Get device size */
    if (ret == 0) {
        dev_size = dev_get_size(fd, &ret);
        if (ret != 0) {
            MOFS_ERR("Get device size error\n");
            goto out2;
        }
    }

    /* Calculate device map */
out2:
    dev_close(fd);
out1:
    return ret;
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

    // MOFS_DBG("Format Configuration:\n");
    // MOFS_DBG("  Target Device: %s\n", device_file);
    // MOFS_DBG("  FS Size:       %d blocks\n", fs_size);

    /* Format device */
    mofs_format(device_file, fs_size, blk_size);

    return 0;
}
