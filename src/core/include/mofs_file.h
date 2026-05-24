#ifndef __MOFS_FILE__
#define __MOFS_FILE__

/*******************************************************
 * includes
 *******************************************************/

#include <mofs_core.h>

/*******************************************************
 * macros
 *******************************************************/

/* File handle pool size */

#define MOFS_FILEHANDLE_POOL_SIZE 64U

/*******************************************************
 * structs
 *******************************************************/

/* File handle */
struct mofs_filehandle
{
    bool         used;
    int          inode_num;
    unsigned int file_offset;
    unsigned int open_flags;
};

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