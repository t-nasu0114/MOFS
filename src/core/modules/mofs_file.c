#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_type.h>
#include <mofs_user.h>

/* File handle pool */
mofs_filehandle_t filehandle_pool[MOFS_FILEHANDLE_POOL_SIZE];

static int get_free_filehandle_index(void)
{
    for (int i = 0; i < MOFS_FILEHANDLE_POOL_SIZE; i++) {
        if (!filehandle_pool[i].used) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Read one data block of a file specified by inode number.
 *
 * Function behavior:
 * - Reads inode metadata for the target file.
 * - Validates requested file block index (`start_blk_num`) against file size.
 * - Reads one filesystem block from the file's data-block mapping.
 * - Returns the valid byte count in the last file block through `fraction`.
 *
 * @param[in] inode_num Target inode number.
 * @param[out] buf Destination buffer for one block of file data.
 * @param[in] start_blk_num File-relative data block index to read.
 * @param[out] fraction Valid bytes in the returned block when reading the
 *                      last partial block; otherwise 0.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid or the block index is out of
 *         file range.
 * @return Non-zero errno value from `get_errno()` on inode/disk read failures.
 */
int read_file_data_block(int inode_num, void *buf, unsigned int start_blk_num, unsigned int req_blk_num,
                         unsigned int *read_blk_num, size_t *fraction)
{
    int          ret = 0;
    mofs_inode_t inode_buf;

    if ((inode_num < 0) || (buf == NULL) || (read_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
        return ret;
    }

    if (start_blk_num >= MOFS_DATA_BLK_PER_FILE) {
        ret = MOFS_EINVAL;
        return ret;
    }

    /* if request block number is 0, return 0 */
    if (req_blk_num == 0) {
        (*read_blk_num) = 0U;
        (*fraction)     = 0U;
        return 0;
    }

    /* check file size and block index */
    if (ret == 0) {
        ret = mofs_read_inode(inode_num, &inode_buf);
        if (ret == 0) {
            if (((inode_buf.i_size + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE) <= start_blk_num) {
                /* exceed file size limit */
                ret = MOFS_EINVAL;
            } else if (start_blk_num + req_blk_num > (inode_buf.i_size + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE) {
                /* trim request block number to fit file size */
                req_blk_num = (inode_buf.i_size + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE - start_blk_num;
            }
        }
    }

    if (ret == 0) {
        (*fraction)     = 0;
        (*read_blk_num) = 0U;

        for (unsigned int i = 0; i < req_blk_num; i++) {
            unsigned int read_one_blk_num = 0U;
            size_t       read_fraction    = 0U;

            ret = read_continuous_blocks(ctx.dev_fd, buf, 1U, inode_buf.i_data_blk[start_blk_num + i],
                                         &read_one_blk_num, &read_fraction);
            if (ret != 0) {
                break;
            }
            if (read_fraction != 0U) {
                *fraction = read_fraction;
                break;
            }
            if (read_one_blk_num != 1U) {
                ret = MOFS_EIO;
                break;
            }

            *read_blk_num = *read_blk_num + 1U;
            buf           = (char *)buf + MOFS_BLK_SIZE;
        }

        if ((ret == 0) && ((*fraction) == 0U)) {
            if (inode_buf.i_size / MOFS_BLK_SIZE < start_blk_num + *read_blk_num) {
                (*fraction) = inode_buf.i_size % MOFS_BLK_SIZE;
                (*read_blk_num) -= 1;
            }
        }
    }

    return ret;
}

/**
 * @brief Write file data blocks mapped by inode block slots.
 *
 * Function behavior:
 * - Reads inode metadata and validates requested file block range.
 * - Writes data one block at a time using `i_data_blk[start_blk_num + i]`.
 * - Reports the number of fully written blocks and short-write remainder.
 * - Does not allocate data blocks; caller must prepare block mapping.
 *
 * @param[in] inode_num Target inode number.
 * @param[in] buf Source buffer containing file data to write.
 * @param[in] start_blk_num File-relative start block index.
 * @param[in] req_blk_num Number of blocks requested to write.
 * @param[out] written_blk_num Number of full blocks successfully written.
 * @param[out] fraction Number of bytes written in a short-write case.
 * @return 0 on success (including short-write case; see `fraction`).
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_ENOSPC if requested range exceeds per-file block limit.
 * @return MOFS_EIO if unexpected block write result is detected.
 * @return Non-zero errno value propagated from inode/block operations.
 */
int write_file_data_block(int inode_num, const void *buf, unsigned int start_blk_num, unsigned int req_blk_num,
                          unsigned int *written_blk_num, size_t *fraction)
{
    int          ret = 0;
    mofs_inode_t inode_buf;

    if ((inode_num < 0) || (buf == NULL) || (written_blk_num == NULL) || (fraction == NULL)) {
        ret = MOFS_EINVAL;
        return ret;
    }

    if (start_blk_num >= MOFS_DATA_BLK_PER_FILE) {
        ret = MOFS_EINVAL;
        return ret;
    }

    /* if request block number is 0, return 0 */
    if (req_blk_num == 0) {
        (*written_blk_num) = 0U;
        (*fraction)        = 0U;
        return 0;
    }

    /* check file size and block index */
    if (ret == 0) {
        ret = mofs_read_inode(inode_num, &inode_buf);
        if (ret == 0) {
            if (req_blk_num > MOFS_DATA_BLK_PER_FILE - start_blk_num) {
                /* trim request block number to fit file size */
                req_blk_num = MOFS_DATA_BLK_PER_FILE - start_blk_num;
            }
        }
    }

    /* Note : DO NOT allocate data block for file here.
     * Allocate data block for file in the caller layer.
     */

    /* write data block to file */
    if (ret == 0) {
        (*fraction)        = 0U;
        (*written_blk_num) = 0U;

        for (unsigned int i = 0; i < req_blk_num; i++) {
            unsigned int written_one_blk_num = 0U;
            size_t       written_fraction    = 0U;
            unsigned int abs_blk_num         = inode_buf.i_data_blk[start_blk_num + i];

            if ((abs_blk_num < ctx.sp_blk.data_region_start) ||
                (ctx.sp_blk.data_region_start + ctx.sp_blk.data_blk_num <= abs_blk_num)) {
                ret = MOFS_EINVAL;
                break;
            }

            ret = write_continuous_blocks(ctx.dev_fd, buf, 1U, abs_blk_num, &written_one_blk_num, &written_fraction);
            if (ret != 0) {
                break;
            }
            if (written_fraction != 0U) {
                *fraction = written_fraction;
                break;
            }
            if (written_one_blk_num != 1U) {
                ret = MOFS_EIO;
                break;
            }

            *written_blk_num = *written_blk_num + 1U;
            buf              = (char *)buf + MOFS_BLK_SIZE;
        }
    }

    return ret;
}

static int check_open_permission(int flags, mofs_user_ctx_t *user, mofs_inode_t *inode)
{
    int          ret             = 0;
    unsigned int open_perm_flags = 0;
    bool         is_determined   = false;

    /* argument NULL check */
    if ((user == NULL) || (inode == NULL)) {
        ret = MOFS_EINVAL;
        goto out;
    }

    /* open flag specified check */
    if ((flags & MOFS_OFLAG_ACCMODE) == 0) {
        ret = MOFS_EINVAL;
        goto out;
    }

    /* check open permission flags validity */
    if ((flags & MOFS_OFLAG_EXEC) == MOFS_OFLAG_EXEC) {
        open_perm_flags = MOFS_OFLAG_EXEC;
        is_determined   = true;
    }
    if ((flags & MOFS_OFLAG_SEARCH) == MOFS_OFLAG_SEARCH) {
        if (is_determined) {
            open_perm_flags = 0;
            ret             = MOFS_EINVAL;
            goto out;
        } else {
            open_perm_flags = MOFS_OFLAG_SEARCH;
            is_determined   = true;
        }
    }
    if ((flags & MOFS_OFLAG_RDONLY) == MOFS_OFLAG_RDONLY) {
        if (is_determined) {
            open_perm_flags = 0;
            ret             = MOFS_EINVAL;
            goto out;
        } else {
            open_perm_flags = MOFS_OFLAG_RDONLY;
            is_determined   = true;
        }
    }
    if ((flags & MOFS_OFLAG_WRONLY) == MOFS_OFLAG_WRONLY) {
        if (is_determined) {
            if (open_perm_flags == MOFS_OFLAG_RDONLY) {
                open_perm_flags = MOFS_OFLAG_RDWR;
            } else {
                open_perm_flags = 0;
                ret             = MOFS_EINVAL;
                goto out;
            }
        } else {
            open_perm_flags = MOFS_OFLAG_WRONLY;
            is_determined   = true;
        }
    }

    /* check open permission */
    if (is_determined) {
        bool         is_member  = false;
        unsigned int is_read    = 0;
        unsigned int is_write   = 0;
        unsigned int is_execute = 0;

        /* check caller user and group permission */
        ret = mofs_is_caller_in_group(inode->i_gid, &is_member);
        if (ret != 0) {
            goto out;
        }
        if (user->uid == inode->i_uid) {
            is_read    = MOFS_S_IRUSR;
            is_write   = MOFS_S_IWUSR;
            is_execute = MOFS_S_IXUSR;
        } else if (is_member) {
            is_read    = MOFS_S_IRGRP;
            is_write   = MOFS_S_IWGRP;
            is_execute = MOFS_S_IXGRP;
        } else {
            is_read    = MOFS_S_IROTH;
            is_write   = MOFS_S_IWOTH;
            is_execute = MOFS_S_IXOTH;
        }

        /* check open permission flags */
        if (open_perm_flags == MOFS_OFLAG_RDONLY) {
            if (inode->i_mode & is_read) {
                ret = 0;
            } else {
                ret = MOFS_EACCES;
                goto out;
            }
        } else if (open_perm_flags == MOFS_OFLAG_WRONLY) {
            if (inode->i_mode & is_write) {
                ret = 0;
            } else {
                ret = MOFS_EACCES;
                goto out;
            }
        } else if (open_perm_flags == MOFS_OFLAG_RDWR) {
            if ((inode->i_mode & is_read) && (inode->i_mode & is_write)) {
                ret = 0;
            } else {
                ret = MOFS_EACCES;
                goto out;
            }
        } else if (open_perm_flags == MOFS_OFLAG_EXEC) {
            if (inode->i_mode & is_execute) {
                ret = 0;
            } else {
                ret = MOFS_EACCES;
                goto out;
            }
        } else if (open_perm_flags == MOFS_OFLAG_SEARCH) {
            if (inode->i_mode & is_execute) {
                ret = 0;
            } else {
                ret = MOFS_EACCES;
                goto out;
            }
        }
    }
out:
    return ret;
}

/**
 * @brief Open a file in MOFS core layer and allocate a file handle.
 *
 * Function behavior:
 * - Validates input pointers and obtains caller user context.
 * - Resolves inode from path and validates file type constraints.
 * - Checks access permissions based on open flags and inode mode bits.
 * - Allocates one entry from file handle pool and initializes it.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[in] flags Open flags (`MOFS_OFLAG_*`).
 * @param[in] mode File mode for creation (currently unused).
 * @param[out] handle Destination pointer for allocated file handle.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments or open flags are invalid.
 * @return MOFS_EPERM or MOFS_EACCES if caller has no permission.
 * @return MOFS_ENOENT if path does not exist and create is not requested.
 * @return MOFS_ENOTSUP if create is requested (not supported yet).
 * @return MOFS_ENOTDIR if directory open is requested for a non-directory.
 * @return MOFS_ENFILE if file handle pool is exhausted.
 * @return Other non-zero errno values propagated from lower layers.
 */
int mofs_open_core(const char *path, int flags, mode_t mode, mofs_filehandle_t **handle)
{
    int             ret       = 0;
    int             inode_num = -1;
    int             index     = -1;
    mofs_inode_t    inode;
    mofs_user_ctx_t user;

    /* Note : currently supports only the absolute path */
    (void)mode;

    if ((path == NULL) || (handle == NULL)) {
        ret = MOFS_EINVAL;
        goto out;
    }

    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        goto out;
    } else if (user.valid == false) {
        ret = MOFS_EPERM;
        goto out;
    }

    ret = mofs_path_to_inode_num(path, &inode_num);
    if (ret == 0) {
        /* Find file inode */
        ret = mofs_read_inode(inode_num, &inode);
        if (ret != 0) {
            goto out;
        }
    } else if ((ret == MOFS_ENOENT) && (flags & MOFS_OFLAG_CREAT)) {
        /* Create file */
        /* FIXME : File creation is not supported yet */
        ret = MOFS_ENOTSUP;
        goto out;
    } else {
        goto out;
    }

    /* validate file type and open flags */
    if (flags & MOFS_OFLAG_DIRECTORY) {
        if (inode.i_mode & MOFS_FTYPE_DIR) {
            ret = 0;
        } else {
            ret = MOFS_ENOTDIR;
            goto out;
        }
    }

    /* check user and permission */
    ret = check_open_permission(flags, &user, &inode);
    if (ret != 0) {
        goto out;
    }

    /* allocate file handle */
    index = get_free_filehandle_index();
    if (index == -1) {
        ret = MOFS_ENFILE;
        goto out;
    }
    *handle                = &filehandle_pool[index];
    (*handle)->used        = true;
    (*handle)->inode_num   = inode_num;
    (*handle)->file_offset = 0;
    (*handle)->open_flags  = flags;
out:
    return ret;
}

/**
 * @brief Close a MOFS file handle and release the pool entry.
 *
 * Function behavior:
 * - Validates handle pointer and confirms the handle is currently in use.
 * - Clears file-handle metadata and marks the pool slot as unused.
 * - Sets caller's handle pointer to NULL.
 *
 * @param[in,out] handle Address of file handle pointer to close.
 * @return 0 on success.
 * @return MOFS_EINVAL if handle pointer is invalid or already unused.
 */
int mofs_close_core(mofs_filehandle_t **handle)
{
    int ret = 0;

    if ((handle == NULL) || (*handle == NULL) || ((*handle)->used == false)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        (*handle)->inode_num   = 0;
        (*handle)->file_offset = 0;
        (*handle)->open_flags  = 0;
        (*handle)->used        = false;
        *handle                = NULL;
    }

    return ret;
}

/**
 * @brief Read file data from current offset in MOFS core layer.
 *
 * Function behavior:
 * - Validates handle state and read arguments.
 * - Verifies read permission from open flags and caller credentials.
 * - Reads one or more data blocks covering requested range.
 * - Copies available bytes to caller buffer and advances `offset`.
 *
 * @param[in,out] handle Address of opened file handle pointer.
 * @param[out] buf Destination buffer for read data.
 * @param[in] size Maximum bytes to read.
 * @param[in,out] offset Byte offset in file; advanced by read bytes on success.
 * @param[out] read_size Actual number of bytes read.
 * @param[in] update_offset If true, updates `handle->file_offset` by read bytes.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EBADF if handle is not opened with read access.
 * @return MOFS_EPERM or MOFS_EACCES if caller has no read permission.
 * @return MOFS_EISDIR if target inode is a directory.
 * @return Other non-zero errno values propagated from lower layers.
 */
int mofs_read_core(mofs_filehandle_t **handle, void *buf, size_t size, off_t *offset, size_t *read_size,
                   bool update_offset)
{
    int             ret = 0;
    mofs_inode_t    inode;
    mofs_user_ctx_t user;
    void           *buf_tmp      = NULL;
    size_t          fraction     = 0;
    unsigned int    read_blk_num = 0;
    unsigned int    req_blk_num  = 0;

    if ((handle == NULL) || (*handle == NULL) || ((*handle)->used == false) || (buf == NULL) || (size == 0) ||
        (offset == NULL) || (*offset < 0) || (read_size == NULL)) {
        ret = MOFS_EINVAL;
        goto out;
    }

    /* check handle open flags and permission */
    if (((*handle)->open_flags & MOFS_OFLAG_RDONLY) == 0) {
        ret = MOFS_EBADF;
        goto out;
    }

    /* read inode */
    ret = mofs_read_inode((*handle)->inode_num, &inode);
    if (ret != 0) {
        goto out;
    }

    /* check user and permission */
    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        goto out;
    } else if (user.valid == false) {
        ret = MOFS_EPERM;
        goto out;
    }
    ret = check_open_permission((*handle)->open_flags, &user, &inode);
    if (ret != 0) {
        goto out;
    }

    /* check file type */
    if (inode.i_mode & MOFS_FTYPE_DIR) {
        ret = MOFS_EISDIR;
        goto out;
    }

    /* start reading */
    req_blk_num = (((*offset % MOFS_BLK_SIZE) + size) + MOFS_BLK_SIZE - 1) / MOFS_BLK_SIZE;
    buf_tmp     = mofs_malloc(req_blk_num * MOFS_BLK_SIZE);
    if (buf_tmp == NULL) {
        ret = get_errno();
        goto out;
    }
    ret = read_file_data_block((*handle)->inode_num, buf_tmp, (*offset) / MOFS_BLK_SIZE, req_blk_num, &read_blk_num,
                               &fraction);
    if (ret != 0) {
        goto out;
    } else {
        if (((read_blk_num * MOFS_BLK_SIZE) + fraction) < ((*offset) % MOFS_BLK_SIZE)) {
            (*read_size) = 0;
        } else {
            (*read_size) = (read_blk_num * MOFS_BLK_SIZE) + fraction - ((*offset) % MOFS_BLK_SIZE);
        }
        if ((*read_size) > size) {
            (*read_size) = size;
        }
        mofs_memcpy(buf, (char *)buf_tmp + ((*offset) % MOFS_BLK_SIZE), (*read_size));
        if (update_offset) {
            (*handle)->file_offset = (unsigned int)(*offset + (off_t)(*read_size));
        }
    }

out:
    if (buf_tmp != NULL) {
        mofs_free(buf_tmp);
    }
    return ret;
}
