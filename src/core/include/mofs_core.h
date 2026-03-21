
#ifndef __MOFS_CORE__
#define __MOFS_CORE__

/*******************************************************
 * includes
 *******************************************************/

#include <mofs_posix.h>
#include <mofs_type.h>

/*******************************************************
 * macros
 *******************************************************/

/* MOFS */
#define MOFS_MAGIC_NUM 0x53464F4DU /* MOFS in ASCII little endian*/

/* Block size */
#define MOFS_BLK_SIZE 4096U

/* inode */

#define MOFS_DATA_BLK_PER_FILE 12U /* Max block number for one file */

/* File type */
#define MOFS_FTYPE_DIR 0040000U /* Directory.  */
#define MOFS_FTYPE_REG 0100000U /* Regular file.  */

/* Directory Entry */

#define MOFS_FILENAME_LEN 28

/* Bool type */
#ifndef bool
#define bool  unsigned int
#define true  1U
#define false 0U
#endif

/*******************************************************
 * structs
 *******************************************************/

/***************************
 * Data
 ****************************/

/* Super Block */
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

/* Inode (64byte aligned) */
typedef struct mofs_inode
{
    uint32_t i_size;  /* File size in bytes */
    uint16_t i_links; /* Link count */
    uint16_t i_mode;  /* Permission and file type */
    uint32_t i_uid;   /* User ID */
    uint32_t i_gid;   /* Group ID */

    uint32_t i_data_blk[MOFS_DATA_BLK_PER_FILE]; /* Absolute block number of data blocks */
} mofs_inode_t;

/* Directory Entry */
typedef struct mofs_dirent
{
    char     name[MOFS_FILENAME_LEN];
    uint32_t inode_num;
} mofs_dirent_t;

/***************************
 * Context
 ****************************/

typedef struct mofs_ctx
{
    bool              init;
    char             *dev_path;
    int               dev_fd;
    mofs_superblock_t sp_blk;
} mofs_ctx_t;

extern mofs_ctx_t ctx;

/*******************************************************
 * functions
 *******************************************************/

int mofs_init_core(const char *path);
int mofs_fini_core(void);
int mofs_stat_core(const char *path, mofs_stat_t *stbuf);

#endif /* __MOFS_CORE__ */
