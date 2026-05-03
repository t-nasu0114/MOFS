#ifndef __MOFS_PATH__
#define __MOFS_PATH__

#include <mofs_file.h>

#define MOFS_PATH_RESOLVE_INODE              0x01U
#define MOFS_PATH_RESOLVE_PARENT             0x02U
#define MOFS_PATH_ALLOW_MISSING_LEAF         0x04U

typedef struct mofs_path_info
{
    int  parent_inode_num;
    int  leaf_inode_num;
    int  leaf_found;
    char leaf_name[MOFS_FILENAME_LEN];
} mofs_path_info_t;

int mofs_resolve_path(const char *path, unsigned int resolve_flags, mofs_path_info_t *path_info);
int mofs_path_to_inode_num(const char *path, int *inode_num);
int mofs_path_to_parent_and_component(const char *path, int *parent_inode_num, char *component);

#endif /* __MOFS_PATH__ */