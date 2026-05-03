#ifndef __MOFS_DIR__
#define __MOFS_DIR__

/*******************************************************
 * includes
 *******************************************************/

#include <mofs_file.h>
#include <mofs_type.h>

/*******************************************************
 * macros
 *******************************************************/

/* Directory handle pool size */
#define MOFS_DIRHANDLE_POOL_SIZE 64U

/*******************************************************
 * structs
 *******************************************************/

/* Directory Entry */
typedef struct mofs_dirent
{
    char     name[MOFS_FILENAME_LEN];
    uint32_t inode_num;
} mofs_dirent_t;

/* Directory handle */
typedef struct mofs_dirhandle
{
    bool          used;
    int           inode_num;
    unsigned int  dirent_offset;
    mofs_dirent_t dirent_buf;
} mofs_dirhandle_t;

/* Directory handle pool */
extern mofs_dirhandle_t dirhandle_pool[MOFS_DIRHANDLE_POOL_SIZE];

/*******************************************************
 * functions
 *******************************************************/

int find_dir_entry(char *component, int parent_inode_num, int *child_inode_num);
int remove_dir_entry(const char *component, int parent_inode_num);
int add_dir_entry(const char *component, int parent_inode_num, int child_inode_num);
int mofs_mkdir_core(const char *path, mode_t mode);
int mofs_rmdir_core(const char *path);

int mofs_opendir_core(const char *path, mofs_dirhandle_t **handle);
int mofs_closedir_core(mofs_dirhandle_t **handle);
int mofs_readdir_core(mofs_dirhandle_t **handle);

#endif /* __MOFS_DIR__ */