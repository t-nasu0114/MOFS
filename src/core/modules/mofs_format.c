
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_mem.h>
#include <mofs_type.h>
#include <mofs_util.h>

/**
 * @brief Zero-fill one block at the specified block index.
 *
 * Function behavior:
 * - Creates a zeroed block buffer.
 * - Seeks to the target block offset.
 * - Writes one full block of zeros to the device.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] block_num Absolute block index to clear.
 * @return 0 on success.
 * @return Non-zero errno value from `get_errno()` on seek/write failure.
 */
static int clear_blocks(int fd, uint64_t block_num)
{
    int   ret = 0;
    char  buf[MOFS_BLK_SIZE];
    off_t offset;

    mofs_memset(buf, 0, MOFS_BLK_SIZE);

    offset = (off_t)block_num * MOFS_BLK_SIZE;

    if ((dev_lseek(fd, offset, MOFS_SEEK_CUR) < 0)) {
        MOFS_ERR("Seek error at block %lu", block_num);
        ret = get_errno();
        return ret;
    }

    if (dev_write(fd, buf, MOFS_BLK_SIZE) != MOFS_BLK_SIZE) {
        MOFS_ERR("Write error at block %lu", block_num);
        ret = get_errno();
        return ret;
    }

    return ret;
}

/**
 * @brief Format a device with MOFS metadata and initialize root directory.
 *
 * Function behavior:
 * - Opens the target device and determines filesystem size.
 * - Computes MOFS layout (superblock, bitmaps, inode table, data region).
 * - Clears metadata region blocks and writes the superblock.
 * - Initializes root inode/data bitmap and writes the root inode entry.
 *
 * @param[in] device_file Path to the target device file.
 * @param[in] fs_size Filesystem size in blocks when > 0; otherwise device size
 *                    is obtained from `dev_get_size()`.
 * @param[in] blk_size Reserved format argument (currently unused).
 * @return 0 on success.
 * @return Non-zero errno value from `get_errno()` on device I/O failures.
 */
int mofs_format(const char *device_file, int fs_size, int blk_size)
{
    int ret = 0;
    int fd;

    (void)blk_size;

    fd = dev_open(device_file, MOFS_IO_OPEN_FLAG_RDWR);
    if (fd < 0) {
        MOFS_ERR("Open %s error\n", device_file);
        ret = get_errno();
        goto out1;
    }

    /* Get device size */
    unsigned long long dev_size;
    if (fs_size > 0) {
        dev_size = (unsigned long long)fs_size * MOFS_BLK_SIZE;
    } else {
        dev_size = dev_get_size(fd, &ret);
        if (ret != 0) {
            MOFS_ERR("Get device size error\n");
            goto out2;
        }
    }

    /* Calculate device layout */
    uint32_t bpi = MOFS_BLK_SIZE * 4; /* Bytes-per-inode = 16KB(4 blocks)
                                    MAYBE: Bytes-per-inode is decided by format option */
    uint64_t hole_blk_num         = dev_size / MOFS_BLK_SIZE;
    uint64_t inode_num            = (dev_size + (bpi - 1)) / bpi;
    uint64_t inode_bitmap_blk_num = (inode_num + MOFS_BLK_SIZE * 8 - 1) / (MOFS_BLK_SIZE * 8);
    uint64_t inode_table_blk_num  = (inode_num * sizeof(mofs_inode_t) + MOFS_BLK_SIZE - 1) / (MOFS_BLK_SIZE);
    uint64_t data_bitmap_blk_num  = (hole_blk_num + MOFS_BLK_SIZE * 8 - 1) / (MOFS_BLK_SIZE * 8);
    uint64_t data_blk_num         = hole_blk_num - inode_bitmap_blk_num - data_bitmap_blk_num - inode_table_blk_num - 1;
    uint64_t meta_region_end      = 1 + inode_bitmap_blk_num + data_bitmap_blk_num + inode_table_blk_num;

    /* Clear super, bitmaps and inode table block */
    for (uint64_t i = 0; i < meta_region_end; i++) {
        ret = clear_blocks(fd, i);
        if (ret != 0) {
            goto out2;
        }
    }

    /* Write superblock */
    struct mofs_superblock superblock;
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

    /* Allocate the No.2 inode for root directory and mark it as used in inode bitmap */
    uint8_t root_inode_bitmap = 0x04; /* Mark the No.2 inode as used. Note that it's not No.0 */
    if (dev_lseek(fd, superblock.inode_bitmap_start * MOFS_BLK_SIZE, MOFS_SEEK_SET) < 0) {
        MOFS_ERR("Seek error at root inode bitmap");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &root_inode_bitmap, 1) != 1) {
        MOFS_ERR("Write error at root inode bitmap");
        ret = get_errno();
        goto out2;
    }

    /* Allocate the first data block for root directory and mark it as used in data bitmap */
    uint8_t root_data_bitmap = 0x01; /* Mark the No.0 data block as used */
    if (dev_lseek(fd, superblock.data_bitmap_start * MOFS_BLK_SIZE, MOFS_SEEK_SET) < 0) {
        MOFS_ERR("Seek error at root data bitmap");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &root_data_bitmap, 1) != 1) {
        MOFS_ERR("Write error at root data bitmap");
        ret = get_errno();
        goto out2;
    }

    /* Write root inode to No.2 inode in table */
    mofs_inode_t root_inode;
    mofs_memset(&root_inode, 0, sizeof(root_inode));
    root_inode.i_size        = MOFS_BLK_SIZE;         /* At least one block size */
    root_inode.i_mode        = MOFS_FTYPE_DIR | 0755; /* Directory with rwx for owner and rx for group and others */
    root_inode.i_links       = 2;                     /* Link count of root directory is 2 (itself and .) */
    root_inode.i_uid         = 0;                     /* User ID of root directory is 0 */
    root_inode.i_gid         = 0;                     /* Group ID of root directory is 0 */
    root_inode.i_data_blk[0] = superblock.data_region_start; /* The first data block is allocated for root directory */

    if (dev_lseek(fd, (superblock.inode_table_start * MOFS_BLK_SIZE) + (2 * sizeof(mofs_inode_t)), MOFS_SEEK_SET) < 0) {
        MOFS_ERR("Seek error at root inode");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &root_inode, sizeof(root_inode)) != sizeof(root_inode)) {
        MOFS_ERR("Write error at root inode");
        ret = get_errno();
        goto out2;
    }
out2:
    dev_close(fd);
out1:
    return ret;
}
