
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
    int                    ret = 0;
    int                    fd;
    int                    hole_blk_num;
    int                    inode_num;
    int                    inode_bitmap_blk_num;
    int                    data_bitmap_blk_num;
    int                    inode_table_blk_num;
    int                    data_blk_num;
    unsigned long long     dev_size;
    struct mofs_superblock superblock;

    (void)blk_size;

    fd = dev_open(device_file, MOFS_IO_OPEN_FLAG_RDWR);
    if (fd < 0) {
        MOFS_ERR("Open %s error\n", device_file);
        ret = get_errno();
        goto out1;
    }

    /* Get device size */
    dev_size = dev_get_size(fd, &ret);
    if (ret != 0) {
        MOFS_ERR("Get device size error\n");
        goto out2;
    }

    /* Calculate device layout */
    int bpi = MOFS_BLK_SIZE * 4; /* Bytes-per-inode = 16KB(4 blocks)
                                    MAYBE: Bytes-per-inode is decided by format option */
    hole_blk_num         = dev_size / MOFS_BLK_SIZE;
    inode_num            = (dev_size + (bpi - 1)) / bpi;
    inode_bitmap_blk_num = (inode_num + MOFS_BLK_SIZE * 8 - 1) / (MOFS_BLK_SIZE * 8);
    inode_table_blk_num  = (inode_num * sizeof(mofs_inode_t) + MOFS_BLK_SIZE - 1) / (MOFS_BLK_SIZE);
    data_bitmap_blk_num  = (hole_blk_num + MOFS_BLK_SIZE * 8 - 1) / (MOFS_BLK_SIZE * 8);
    data_blk_num         = hole_blk_num - inode_bitmap_blk_num - data_bitmap_blk_num - inode_table_blk_num - 1;

    /* Clear super, bitmaps and inode table block */
    for (int i = 0; i < data_blk_num; i++) {
        ret = clear_blocks(fd, i);
        if (ret != 0) {
            goto out2;
        }
    }

    /* Write superblock */
    superblock.magic              = MOFS_MAGIC_NUM;
    superblock.hole_blk_num       = dev_size / MOFS_BLK_SIZE;
    superblock.inode_num          = inode_num;
    superblock.data_blk_num       = data_blk_num;
    superblock.inode_bitmap_start = 1;
    superblock.data_bitmap_start  = superblock.inode_bitmap_start + inode_bitmap_blk_num;
    superblock.inode_table_start  = superblock.data_bitmap_start + data_bitmap_blk_num;
    superblock.data_region_start  = superblock.inode_table_start + inode_table_blk_num;

    if (dev_lseek(fd, 0, MOFS_SEEK_SET) < 0) {
        MOFS_ERR("Seek error at superblock");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &superblock, sizeof(superblock)) != sizeof(superblock)) {
        MOFS_ERR("Write error at superblock");
        ret = get_errno();
        goto out2;
    }

    /* Make Root Directory */
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
