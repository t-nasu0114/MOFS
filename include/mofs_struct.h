
#ifndef __MOFS_STRUCT__
#define __MOFS_STRUCT__

#include <stdint.h>

/* Super Block */

#define MOFS_BLK_SIZE  4096U
#define MOFS_MAGIC_NUM 0xMOFS

typedef struct mofs_superblock
{
    uint32_t magic;        /* Magic Number of My Original File System */
    uint32_t hole_blk_num; /* Block number of hole volume */
    uint32_t inode_num;    /* inode number */
    uint32_t data_blk_num; /* block Number of data block */

    /* Start block number of each area */
    uint32_t inode_bitmap_start;
    uint32_t data_bitmap_start;
    uint32_t inode_table_start;
    uint32_t data_region_start;
} mofs_superblock_t;

/* inode */

#define MOFS_DATA_BLK_PER_FILE 12U /* Max block number for one file */

typedef enum mofs_filetype
{
    MOFS_DIR  = 1,
    MOFS_FILE = 2
} mofs_filetype_e;

typedef struct mofs_inode
{
    mofs_filetype_e i_type;
    uint32_t        i_size;
    uint16_t        i_links;

    uint32_t i_start_blk[MOFS_DATA_BLK_PER_FILE];
} mofs_inode_t;

/* Directory Entry */

#define MOFS_FILENAME_LEN 28

typedef struct mofs_dirent
{
    char     name[MOFS_FILENAME_LEN];
    uint32_t inode_num;
} mofs_dirent_t;

#endif /* __MOFS_STRUCT__ */
