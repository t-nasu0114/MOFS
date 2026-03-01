
#ifndef __MOFS_STRUCT__
#define __MOFS_STRUCT__

#include <stdint.h>

/* Super Block */

#define MOFS_BLK_SIZE  4096U
#define MOFS_MAGIC_NUM 0x4D4F4653U /* MOFS in ASCII */

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

/* File type */
#define MOFS_FTYPE_DIR 1U
#define MOFS_FTYPE_REG 2U

/* Inode (64byte aligned) */
typedef struct mofs_inode
{
    uint32_t i_size;  /* File size in bytes */
    uint16_t i_links; /* Link count */
    uint16_t i_mode;  /* Permission and file type */
    uint32_t i_uid;   /* User ID */
    uint32_t i_gid;   /* Group ID */

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
