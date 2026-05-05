#ifndef __MOFS_FORMAT__
#define __MOFS_FORMAT__

/**
 * @param blk_size Logical block size in bytes, or `-1` to use MOFS_BLK_SIZE_DEFAULT (4096).
 */
int mofs_format(const char *device_file, int fs_size, int blk_size);

#endif /* __MOFS_FORMAT__ */