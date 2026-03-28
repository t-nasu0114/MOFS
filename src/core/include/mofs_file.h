#ifndef __MOFS_FILE__
#define __MOFS_FILE__

/*******************************************************
 * includes
 *******************************************************/

#include <mofs_type.h>

/*******************************************************
 * macros
 *******************************************************/

/* File type */

#define MOFS_FTYPE_DIR 0040000U /* Directory.  */
#define MOFS_FTYPE_REG 0100000U /* Regular file.  */

/* Directory Entry */

#define MOFS_FILENAME_LEN 28

/*******************************************************
 * functions
 *******************************************************/

int read_file_data_block(int inode_num, void *buf, unsigned int start_blk_num, size_t *fraction);

#endif /* __MOFS_FILE__ */