
#ifndef __MOFS_CORE__
#define __MOFS_CORE__

/*******************************************************
 * includes
 *******************************************************/

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

/* Bool type */
#ifndef bool
#define bool  unsigned int
#define true  1U
#define false 0U
#endif

/* Access permission flags */
#define MOFS_S_IRUSR   0000400U
#define MOFS_S_IWUSR   0000200U
#define MOFS_S_IXUSR   0000100U
#define MOFS_S_IRWXUSR (MOFS_S_IRUSR | MOFS_S_IWUSR | MOFS_S_IXUSR)
#define MOFS_S_IRGRP   0000040U
#define MOFS_S_IWGRP   0000020U
#define MOFS_S_IXGRP   0000010U
#define MOFS_S_IRWXGRP (MOFS_S_IRGRP | MOFS_S_IWGRP | MOFS_S_IXGRP)
#define MOFS_S_IROTH   0000004U
#define MOFS_S_IWOTH   0000002U
#define MOFS_S_IXOTH   0000001U
#define MOFS_S_IRWXOTH (MOFS_S_IROTH | MOFS_S_IWOTH | MOFS_S_IXOTH)
#define MOFS_S_ISUID   0004000U
#define MOFS_S_ISGID   0002000U
#define MOFS_S_ISVTX   0001000U

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

/* File status */
typedef struct mofs_stat
{
    ino_t   st_ino;
    mode_t  st_mode;
    nlink_t st_nlink;
    uid_t   st_uid;
    gid_t   st_gid;
    off_t   st_size;
    /* Time info is not supported yet */
    uint32_t st_blocks;
} mofs_stat_t;

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
