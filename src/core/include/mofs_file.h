#ifndef __MOFS_FILE__
#define __MOFS_FILE__

/*******************************************************
 * includes
 *******************************************************/

#include <mofs_core.h>

/*******************************************************
 * macros
 *******************************************************/

/* File name length */

#define MOFS_FILENAME_LEN 28

/* File handle pool size */

#define MOFS_FILEHANDLE_POOL_SIZE 64U

/* File type */

#define MOFS_FTYPE_DIR 0040000U /* Directory.  */
#define MOFS_FTYPE_REG 0100000U /* Regular file.  */

/***************************
 * Supported open flags
 ****************************/

/* Essential flags */

#define MOFS_OFLAG_RDONLY  0x0001
#define MOFS_OFLAG_WRONLY  0x0002
#define MOFS_OFLAG_RDWR    (MOFS_OFLAG_RDONLY | MOFS_OFLAG_WRONLY)
#define MOFS_OFLAG_EXEC    0x0004
#define MOFS_OFLAG_SEARCH  0x0008
#define MOFS_OFLAG_ACCMODE (MOFS_OFLAG_RDWR | MOFS_OFLAG_EXEC | MOFS_OFLAG_SEARCH)

/* Other flags */

#define MOFS_OFLAG_CREAT     0x0010  // Support is TBD
#define MOFS_OFLAG_DIRECTORY 0x0020
#define MOFS_OFLAG_EXCL      0x0040  // Support is TBD
#define MOFS_OFLAG_TRUNC     0x0080  // Support is TBD
#define MOFS_OFLAG_APPEND    0x0100  // Support is TBD
#define MOFS_OFLAG_SYNC      0x0200  // Support is TBD

/*******************************************************
 * structs
 *******************************************************/

/* File handle */
typedef struct mofs_filehandle
{
    bool         used;
    int          inode_num;
    unsigned int file_offset;
    unsigned int open_flags;
} mofs_filehandle_t;

/* File handle pool */
extern mofs_filehandle_t filehandle_pool[MOFS_FILEHANDLE_POOL_SIZE];

/*******************************************************
 * functions
 *******************************************************/

/* internal utilities */
int read_file_data_block(int inode_num, void *buf, unsigned int start_blk_num, unsigned int req_blk_num,
                         unsigned int *read_blk_num, size_t *fraction);
int write_file_data_block(int inode_num, const void *buf, unsigned int start_blk_num, unsigned int req_blk_num,
                          unsigned int *written_blk_num, size_t *fraction);

/* external functions */
int mofs_open_core(const char *path, int flags, mode_t mode, mofs_filehandle_t **handle);
int mofs_close_core(mofs_filehandle_t **handle);
int mofs_create_core(const char *path, mode_t mode, int *inode_num);
int mofs_unlink_core(const char *path);
int mofs_stat_core(const char *path, mofs_stat_t *stbuf);
int mofs_read_core(mofs_filehandle_t **handle, void *buf, size_t size, off_t *offset, size_t *read_size,
                   bool update_offset);
int mofs_write_core(mofs_filehandle_t **handle, const void *buf, size_t size, off_t *offset, size_t *written_size,
                    bool update_offset);

#endif /* __MOFS_FILE__ */