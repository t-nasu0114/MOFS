#include <mofs_port_errno.h>

#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_file.h>
#include <mofs_port_mem.h>
#include <mofs_port_time.h>
#include <mofs_types.h>
#include <mofs_port_log.h>

/**
 * @brief Zero-fill one block at the specified block index.
 *
 * @param[in] fd Device file descriptor.
 * @param[in] block_num Absolute block index to clear.
 * @param[in] blk_bytes Logical block size in bytes.
 * @return 0 on success.
 * @return Non-zero errno value from `get_errno()` on seek/write failure.
 */
static int clear_blocks(int fd, mofs_uint64_t block_num, mofs_uint32_t blk_bytes)
{
    int   ret    = 0;
    void *buf    = NULL;
    mofs_off_t offset = (mofs_off_t)((mofs_uint64_t)block_num * (mofs_uint64_t)blk_bytes);

    buf = mofs_malloc((mofs_size_t)blk_bytes);
    if (buf == NULL) {
        return get_errno();
    }

    mofs_memset(buf, 0, (mofs_size_t)blk_bytes);

    if (dev_lseek(fd, offset, MOFS_SEEK_SET) < 0) {
        mofs_log_err("Seek error at block %lu", (unsigned long)block_num);
        ret = get_errno();
        mofs_free(buf);
        return ret;
    }

    if (dev_write(fd, buf, (mofs_size_t)blk_bytes) != (int)(mofs_size_t)blk_bytes) {
        mofs_log_err("Write error at block %lu", (unsigned long)block_num);
        ret = get_errno();
        mofs_free(buf);
        return ret;
    }

    mofs_free(buf);
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
 * @param[in] blk_size Logical block size in bytes; if negative, MOFS_BLK_SIZE_DEFAULT is used.
 * @return 0 on success.
 * @return MOFS_EINVAL if `blk_size` is unsupported or layout is impossible.
 * @return Non-zero errno value from `get_errno()` on device I/O failures.
 */
int mofs_format(const char *device_file, int fs_size, int blk_size)
{
    int                ret           = 0;
    int                fd            = -1;
    mofs_uint32_t           eff_blk_bytes = MOFS_BLK_SIZE_DEFAULT;
    unsigned long long dev_size;

    fd = dev_open(device_file, MOFS_IO_OPEN_FLAG_RDWR);
    if (fd < 0) {
        mofs_log_err("Open %s error\n", device_file);
        ret = get_errno();
        goto out1;
    }

    if (blk_size >= 0) {
        eff_blk_bytes = (mofs_uint32_t)blk_size;
    }

    ret = mofs_validate_logical_blk_size(eff_blk_bytes);
    if (ret != 0) {
        mofs_log_err("Unsupported block size %u\n", eff_blk_bytes);
        goto out2;
    }

    /* Get device size */
    if (fs_size > 0) {
        dev_size = (unsigned long long)(unsigned int)fs_size * (unsigned long long)eff_blk_bytes;
    } else {
        dev_size = dev_get_size(fd, &ret);
        if (ret != 0) {
            mofs_log_err("Get device size error\n");
            goto out2;
        }
        /* Device tail bytes smaller than one logical block are ignored. */
    }

    /* Calculate device layout */
    mofs_uint32_t bpi = eff_blk_bytes * 4U; /* Bytes-per-inode = 16KB equivalent at default 4K blocks */

    mofs_uint64_t hole_blk_num = dev_size / (unsigned long long)eff_blk_bytes;
    mofs_uint64_t inode_num    = (dev_size + (unsigned long long)(bpi - 1U)) / (unsigned long long)bpi;
    mofs_uint64_t inode_bitmap_blk_num =
        (inode_num + (mofs_uint64_t)eff_blk_bytes * 8ULL - 1ULL) / ((mofs_uint64_t)eff_blk_bytes * 8ULL);
    mofs_uint64_t inode_table_blk_num =
        (inode_num * sizeof(mofs_inode_t) + (mofs_uint64_t)eff_blk_bytes - 1ULL) / (mofs_uint64_t)eff_blk_bytes;
    mofs_uint64_t data_bitmap_blk_num =
        (hole_blk_num + (mofs_uint64_t)eff_blk_bytes * 8ULL - 1ULL) / ((mofs_uint64_t)eff_blk_bytes * 8ULL);
    mofs_uint64_t meta_reserved = 1ULL + inode_bitmap_blk_num + data_bitmap_blk_num + inode_table_blk_num;
    mofs_uint64_t data_blk_num  = 0;

    if (hole_blk_num <= meta_reserved) {
        ret = MOFS_EINVAL;
        mofs_log_err("Device too small for MOFS metadata with this block size\n");
        goto out2;
    }

    data_blk_num = hole_blk_num - meta_reserved;

    /* Clear super, bitmaps and inode table block */
    for (mofs_uint64_t i = 0; i < meta_reserved; i++) {
        ret = clear_blocks(fd, i, eff_blk_bytes);
        if (ret != 0) {
            goto out2;
        }
    }

    /* Write superblock */
    mofs_superblock_t superblock;
    mofs_memset(&superblock, 0, sizeof(superblock));
    superblock.magic              = MOFS_MAGIC_NUM;
    superblock.hole_blk_num       = (mofs_uint32_t)hole_blk_num;
    superblock.inode_num          = (mofs_uint32_t)inode_num;
    superblock.data_blk_num       = (mofs_uint32_t)data_blk_num;
    superblock.inode_bitmap_start = 1U;
    superblock.data_bitmap_start  = superblock.inode_bitmap_start + (mofs_uint32_t)inode_bitmap_blk_num;
    superblock.inode_table_start  = superblock.data_bitmap_start + (mofs_uint32_t)data_bitmap_blk_num;
    superblock.data_region_start  = superblock.inode_table_start + (mofs_uint32_t)inode_table_blk_num;
    superblock.blk_size           = eff_blk_bytes;

    if (hole_blk_num > (mofs_uint64_t)MOFS_UINT32_MAX) {
        ret = MOFS_EINVAL;
        goto out2;
    }

    if (dev_lseek(fd, 0, MOFS_SEEK_SET) < 0) {
        mofs_log_err("Seek error at superblock");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &superblock, sizeof(superblock)) != (int)sizeof(superblock)) {
        mofs_log_err("Write error at superblock");
        ret = get_errno();
        goto out2;
    }

    /* Make Root Directory */

    /* Allocate the No.2 inode for root directory and mark it as used in inode bitmap */
    mofs_uint8_t root_inode_bitmap = 0x04; /* Mark the No.2 inode as used. Note that it's not No.0 */
    if (dev_lseek(fd, (mofs_off_t)((mofs_uint64_t)superblock.inode_bitmap_start * (mofs_uint64_t)eff_blk_bytes), MOFS_SEEK_SET) < 0) {
        mofs_log_err("Seek error at root inode bitmap");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &root_inode_bitmap, 1) != 1) {
        mofs_log_err("Write error at root inode bitmap");
        ret = get_errno();
        goto out2;
    }

    /* Allocate data block 0 (dir content) and block 1 (list node); mark both used */
    mofs_uint8_t root_data_bitmap = 0x03;
    if (dev_lseek(fd, (mofs_off_t)((mofs_uint64_t)superblock.data_bitmap_start * (mofs_uint64_t)eff_blk_bytes), MOFS_SEEK_SET) < 0) {
        mofs_log_err("Seek error at root data bitmap");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &root_data_bitmap, 1) != 1) {
        mofs_log_err("Write error at root data bitmap");
        ret = get_errno();
        goto out2;
    }

    /* Clear root directory data block to avoid stale dirents from old images. */
    ret = clear_blocks(fd, superblock.data_region_start, eff_blk_bytes);
    if (ret != 0) {
        mofs_log_err("Clear error at root directory data block");
        goto out2;
    }

    /* List node at data block 1 points to directory data block 0. */
    ret = clear_blocks(fd, superblock.data_region_start + 1ULL, eff_blk_bytes);
    if (ret != 0) {
        mofs_log_err("Clear error at root list node block");
        goto out2;
    }

    {
        void                  *list_buf = mofs_malloc((mofs_size_t)eff_blk_bytes);
        mofs_data_list_hdr_t  *list_hdr;
        mofs_uint32_t              *list_ptr;

        if (list_buf == NULL) {
            ret = get_errno();
            goto out2;
        }
        mofs_memset(list_buf, 0, (mofs_size_t)eff_blk_bytes);
        list_hdr           = (mofs_data_list_hdr_t *)list_buf;
        list_hdr->next_abs = 0U;
        list_hdr->nr_ptrs  = 1U;
        /* Single pointer: root directory content block. */
        list_ptr           = (mofs_uint32_t *)((unsigned char *)list_buf + sizeof(mofs_data_list_hdr_t));
        list_ptr[0]        = superblock.data_region_start;

        if (dev_lseek(fd,
                      (mofs_off_t)((mofs_uint64_t)(superblock.data_region_start + 1ULL) * (mofs_uint64_t)eff_blk_bytes),
                      MOFS_SEEK_SET) < 0) {
            mofs_log_err("Seek error at root list node");
            ret = get_errno();
            mofs_free(list_buf);
            goto out2;
        }
        if (dev_write(fd, list_buf, (mofs_size_t)eff_blk_bytes) != (int)(mofs_size_t)eff_blk_bytes) {
            mofs_log_err("Write error at root list node");
            ret = get_errno();
            mofs_free(list_buf);
            goto out2;
        }
        mofs_free(list_buf);
    }

    /* Write root inode to No.2 inode in table */
    mofs_inode_t root_inode;
    mofs_memset(&root_inode, 0, sizeof(root_inode));
    root_inode.i_size      = eff_blk_bytes;
    root_inode.i_mode      = MOFS_FTYPE_DIR | 0755;
    root_inode.i_links     = 2;
    root_inode.i_uid       = 0;
    root_inode.i_gid       = 0;
    root_inode.i_data_head = superblock.data_region_start + 1U;
    root_inode.i_nr_blocks = 1U;
    {
        mofs_time_sec_t now = MOFS_TIME_INVALID;

        ret = mofs_now(&now);
        if (ret != 0) {
            mofs_log_err("Failed to set root inode timestamps");
            goto out2;
        }
        root_inode.i_atime = now;
        root_inode.i_mtime = now;
        root_inode.i_ctime = now;
    }

    if (dev_lseek(fd,
                  (mofs_off_t)((mofs_uint64_t)superblock.inode_table_start * (mofs_uint64_t)eff_blk_bytes) +
                      (mofs_off_t)(2 * sizeof(mofs_inode_t)),
                  MOFS_SEEK_SET) < 0) {
        mofs_log_err("Seek error at root inode");
        ret = get_errno();
        goto out2;
    }

    if (dev_write(fd, &root_inode, sizeof(root_inode)) != (int)sizeof(root_inode)) {
        mofs_log_err("Write error at root inode");
        ret = get_errno();
        goto out2;
    }
out2:
    dev_close(fd);
out1:
    return ret;
}
