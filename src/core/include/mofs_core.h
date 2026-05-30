
#ifndef __MOFS_CORE__
#define __MOFS_CORE__

/*******************************************************
 * includes
 *******************************************************/

#include <mofs_errno.h>
#include <mofs_format.h>
#include <mofs_lifecycle.h>
#include <mofs_posix.h>
#include <mofs_type.h>

/*******************************************************
 * macros
 *******************************************************/

/* MOFS */
#define MOFS_MAGIC_NUM 0x53464F4DU /* MOFS in ASCII little endian*/

/* Logical block size:
 * - Default when `mofs_format(..., blk_size)` passes -1.
 * - Validates against MOFS_BLK_SIZE_MIN and MOFS_BLK_SIZE_MAX.
 * - Power-of-two check: valid values have a single set bit (512 * (2 ^ n)).
 */
#define MOFS_BLK_SIZE_DEFAULT 4096U
#define MOFS_BLK_SIZE         MOFS_BLK_SIZE_DEFAULT
#define MOFS_BLK_SIZE_MIN     512U
#define MOFS_BLK_SIZE_MAX     65536U

static inline int mofs_validate_logical_blk_size(uint32_t blk_size)
{
    if ((blk_size < MOFS_BLK_SIZE_MIN) || (blk_size > MOFS_BLK_SIZE_MAX)) {
        return MOFS_EINVAL;
    }
    /* Power-of-two check: valid values have a single set bit (512 * (2 ^ n)). */
    if ((blk_size & (blk_size - 1U)) != 0U) {
        return MOFS_EINVAL;
    }
    return 0;
}

/* inode / file data blocks */

#define MOFS_MAX_FILE_DATA_BLOCKS 1024U /* Max data blocks per file (list nodes excluded) */

/** Number of file data pointers that fit in one on-disk list node block. */
static inline unsigned int mofs_list_ptrs_per_node(uint32_t blk_size)
{
    unsigned int hdr_bytes = 8U; /* mofs_data_list_hdr: next_abs + nr_ptrs */
    if (blk_size <= hdr_bytes) {
        return 0U;
    }
    return (blk_size - hdr_bytes) / 4U;
}

/** Maximum file size in bytes after mount (`MOFS_MAX_FILE_DATA_BLOCKS * blk_size`). */
uint64_t mofs_max_file_bytes(void);

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

    uint32_t blk_size; /* Logical block size in bytes */
} mofs_superblock_t;

/* On-disk list node header (one per list block in data region) */
typedef struct mofs_data_list_hdr
{
    uint32_t next_abs; /* Absolute block of next list node, 0 if none */
    uint32_t nr_ptrs;  /* Number of valid data block pointers following header */
} mofs_data_list_hdr_t;

/* Inode (64byte aligned) */
typedef struct mofs_inode
{
    uint32_t i_size;      /* File size in bytes */
    uint16_t i_links;     /* Link count */
    uint16_t i_mode;      /* Permission and file type */
    uint32_t i_uid;       /* User ID */
    uint32_t i_gid;       /* Group ID */
    uint32_t i_data_head; /* Absolute block of first list node, 0 if no data mapping */
    uint32_t i_nr_blocks; /* Number of file data blocks (not list nodes) */
    uint32_t reserved[8]; /* Padding to 64 bytes */
} mofs_inode_t;

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

#endif /* __MOFS_CORE__ */
