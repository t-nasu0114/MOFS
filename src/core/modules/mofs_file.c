#include "mofs_block.h"
#include <mofs_core.h>
#include <mofs_devio.h>
#include <mofs_dir.h>
#include <mofs_errno.h>
#include <mofs_file.h>
#include <mofs_inode.h>
#include <mofs_mem.h>
#include <mofs_path.h>
#include <mofs_perm.h>
#include <mofs_type.h>
#include <mofs_user.h>

/* File handle pool */
mofs_filehandle_t filehandle_pool[MOFS_FILEHANDLE_POOL_SIZE];

/* internal functions */
static int zero_partial_block_tail(int inode_num, unsigned int blk_idx, size_t tail_start);
static int zero_file_byte_range(int inode_num, uint32_t from_off, uint32_t to_off);
static int truncate_inode(int inode_num, uint32_t new_size);

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
 * - Validates `start_blk_num` against `i_nr_blocks` and file size.
 * - Resolves each logical block via `resolve_file_data_block()`.
 * - Reads one filesystem block per resolved absolute block number.
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

    /* if request block number is 0, return 0 */
    if (req_blk_num == 0) {
        (*read_blk_num) = 0U;
        (*fraction)     = 0U;
        return 0;
    }

    /* check mapped block count and trim read range to valid file size */
    if (ret == 0) {
        ret = mofs_read_inode(inode_num, &inode_buf);
        if (ret == 0) {
            if (start_blk_num >= inode_buf.i_nr_blocks) {
                ret = MOFS_EINVAL;
            } else if (((inode_buf.i_size + ctx.sp_blk.blk_size - 1) / ctx.sp_blk.blk_size) <= start_blk_num) {
                ret = MOFS_EINVAL;
            } else if (start_blk_num + req_blk_num >
                       (inode_buf.i_size + ctx.sp_blk.blk_size - 1) / ctx.sp_blk.blk_size) {
                req_blk_num = (inode_buf.i_size + ctx.sp_blk.blk_size - 1) / ctx.sp_blk.blk_size - start_blk_num;
            }
        }
    }

    if (ret == 0) {
        (*fraction)     = 0;
        (*read_blk_num) = 0U;

        for (unsigned int i = 0; i < req_blk_num; i++) {
            unsigned int read_one_blk_num = 0U;
            size_t       read_fraction    = 0U;
            unsigned int abs_blk_num      = 0U;

            /* Logical index -> absolute data block via on-disk list nodes. */
            ret = resolve_file_data_block(inode_num, start_blk_num + i, &abs_blk_num);
            if (ret != 0) {
                break;
            }

            ret = read_continuous_blocks(ctx.dev_fd, buf, 1U, abs_blk_num, &read_one_blk_num, &read_fraction);
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
            buf           = (char *)buf + ctx.sp_blk.blk_size;
        }

        if ((ret == 0) && ((*fraction) == 0U)) {
            if (inode_buf.i_size / ctx.sp_blk.blk_size < start_blk_num + *read_blk_num) {
                (*fraction) = inode_buf.i_size % ctx.sp_blk.blk_size;
                (*read_blk_num) -= 1;
            }
        }
    }

    return ret;
}

/**
 * @brief Write file data blocks through the on-disk block list mapping.
 *
 * Function behavior:
 * - Reads inode metadata and validates the requested range against `i_nr_blocks`.
 * - Resolves each logical block via `resolve_file_data_block()`.
 * - Writes one filesystem block per resolved absolute block number.
 * - Does not allocate data blocks; caller must extend mapping first.
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

    /* if request block number is 0, return 0 */
    if (req_blk_num == 0) {
        (*written_blk_num) = 0U;
        (*fraction)        = 0U;
        return 0;
    }

    /* check mapped block range (allocation is done by the caller) */
    if (ret == 0) {
        ret = mofs_read_inode(inode_num, &inode_buf);
        if (ret == 0) {
            if (start_blk_num >= inode_buf.i_nr_blocks) {
                ret = MOFS_EINVAL;
            } else if (req_blk_num > (inode_buf.i_nr_blocks - start_blk_num)) {
                req_blk_num = inode_buf.i_nr_blocks - start_blk_num;
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
            unsigned int abs_blk_num         = 0U;

            ret = resolve_file_data_block(inode_num, start_blk_num + i, &abs_blk_num);
            if (ret != 0) {
                break;
            }

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
            buf              = (char *)buf + ctx.sp_blk.blk_size;
        }
    }

    return ret;
}

/**
 * @brief Create a regular file entry and inode in MOFS core layer.
 *
 * Function behavior:
 * - Resolves parent directory and leaf component from `path`.
 * - Returns `MOFS_EEXIST` when the target path already exists.
 * - Allocates and initializes a new inode for an empty regular file.
 * - Adds a directory entry under the resolved parent directory.
 * - Rolls back allocated inode when creation fails before link completion.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[in] mode Permission bits for the new file.
 * @param[out] inode_num Destination pointer for created inode number.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EEXIST if target path already exists.
 * @return MOFS_EPERM if caller user context is unavailable.
 * @return Non-zero errno value propagated from path/inode/dir helpers.
 */
int mofs_create_core(const char *path, mode_t mode, int *inode_num)
{
    int              ret                 = 0;
    int              allocated_inode_num = -1;
    bool             inode_allocated     = false;
    bool             dirent_linked       = false;
    mofs_inode_t     inode_buf;
    mofs_inode_t     parent_inode;
    mofs_user_ctx_t  user;
    mofs_path_info_t path_info;

    if ((path == NULL) || (inode_num == NULL)) {
        return MOFS_EINVAL;
    }
    *inode_num = -1;

    mofs_memset(&path_info, 0, sizeof(path_info));
    ret = mofs_resolve_path(path,
                            MOFS_PATH_RESOLVE_PARENT | MOFS_PATH_RESOLVE_INODE | MOFS_PATH_ALLOW_MISSING_LEAF |
                                MOFS_PATH_CHECK_ACCESS,
                            &path_info);
    if (ret != 0) {
        return ret;
    }
    if (path_info.leaf_found) {
        return MOFS_EEXIST;
    }

    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        return ret;
    }
    if (user.valid == false) {
        return MOFS_EPERM;
    }

    ret = mofs_read_inode(path_info.parent_inode_num, &parent_inode);
    if (ret != 0) {
        return ret;
    }
    ret = mofs_check_dir_write(&user, &parent_inode);
    if (ret != 0) {
        return ret;
    }

    ret = allocate_inode(&allocated_inode_num);
    if (ret != 0) {
        goto rollback;
    }
    inode_allocated = true;

    mofs_memset(&inode_buf, 0, sizeof(inode_buf));
    inode_buf.i_size  = 0U;
    inode_buf.i_links = 1U;
    inode_buf.i_mode  = (uint16_t)(MOFS_FTYPE_REG | (mode & 0777U));
    inode_buf.i_uid   = user.uid;
    inode_buf.i_gid   = user.gid;
    ret               = mofs_inode_stamp_now(&inode_buf, MOFS_INODE_TIME_ALL);
    if (ret != 0) {
        goto rollback;
    }

    ret = mofs_write_inode(allocated_inode_num, &inode_buf);
    if (ret != 0) {
        goto rollback;
    }

    ret = add_dir_entry(path_info.leaf_name, path_info.parent_inode_num, allocated_inode_num);
    if (ret != 0) {
        goto rollback;
    }
    dirent_linked = true;

    *inode_num = allocated_inode_num;
    return 0;

rollback:
    if (inode_allocated && (dirent_linked == false)) {
        (void)free_inode(allocated_inode_num);
    }
    return ret;
}

/**
 * @brief Unlink a regular file from a directory path.
 *
 * Function behavior:
 * - Resolves parent directory inode and target entry name from `path`.
 * - Resolves target inode and rejects directory targets.
 * - Removes directory entry first (no recovery after this point).
 * - Frees file data blocks and inode bitmap entry.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @return 0 on success.
 * @return MOFS_EINVAL if path is invalid or root path is specified.
 * @return MOFS_ENOENT if path target does not exist.
 * @return MOFS_EISDIR if target is a directory.
 * @return Non-zero errno value propagated from lower helpers.
 */
int mofs_unlink_core(const char *path)
{
    int              ret              = 0;
    int              target_inode_num = -1;
    unsigned int     used_blk_num     = 0U;
    mofs_inode_t     target_inode;
    mofs_inode_t     parent_inode;
    mofs_user_ctx_t  user;
    mofs_path_info_t path_info;

    mofs_memset(&path_info, 0, sizeof(path_info));
    ret = mofs_resolve_path(path, MOFS_PATH_RESOLVE_PARENT | MOFS_PATH_RESOLVE_INODE | MOFS_PATH_CHECK_ACCESS,
                            &path_info);
    if (ret != 0) {
        return ret;
    }
    target_inode_num = path_info.leaf_inode_num;

    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        return ret;
    }
    if (user.valid == false) {
        return MOFS_EPERM;
    }

    ret = mofs_read_inode(path_info.parent_inode_num, &parent_inode);
    if (ret != 0) {
        return ret;
    }
    ret = mofs_check_dir_write(&user, &parent_inode);
    if (ret != 0) {
        return ret;
    }

    ret = mofs_read_inode(target_inode_num, &target_inode);
    if (ret != 0) {
        return ret;
    }
    if ((target_inode.i_mode & MOFS_FTYPE_DIR) != 0U) {
        return MOFS_EISDIR;
    }

    ret = remove_dir_entry(path_info.leaf_name, path_info.parent_inode_num);
    if (ret != 0) {
        return ret;
    }

    used_blk_num = target_inode.i_nr_blocks;
    if (used_blk_num > 0U) {
        /* Free all mapped data blocks and list nodes via compact remove from index 0. */
        ret = free_data_block(target_inode_num, 0U, used_blk_num);
        if (ret != 0) {
            return ret;
        }
    }

    ret = free_inode(target_inode_num);
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
 * @param[in] mode File mode for creation path.
 * @param[out] handle Destination pointer for allocated file handle.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments or open flags are invalid.
 * @return MOFS_EPERM or MOFS_EACCES if caller has no permission.
 * @return MOFS_ENOENT if path does not exist and create is not requested.
 * @return MOFS_EEXIST if `MOFS_OFLAG_CREAT | MOFS_OFLAG_EXCL` is specified
 *         for an already existing file.
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
        if ((flags & MOFS_OFLAG_CREAT) && (flags & MOFS_OFLAG_EXCL)) {
            ret = MOFS_EEXIST;
            goto out;
        }
        /* Find file inode */
        ret = mofs_read_inode(inode_num, &inode);
        if (ret != 0) {
            goto out;
        }
    } else if ((ret == MOFS_ENOENT) && (flags & MOFS_OFLAG_CREAT)) {
        ret = mofs_create_core(path, mode, &inode_num);
        if (ret != 0) {
            goto out;
        }
        ret = mofs_read_inode(inode_num, &inode);
        if (ret != 0) {
            goto out;
        }
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
    ret = mofs_check_open_permission(flags, &user, &inode);
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
    ret = mofs_check_open_permission((*handle)->open_flags, &user, &inode);
    if (ret != 0) {
        goto out;
    }

    /* check file type */
    if (inode.i_mode & MOFS_FTYPE_DIR) {
        ret = MOFS_EISDIR;
        goto out;
    }

    /* start reading */
    req_blk_num = (((*offset % ctx.sp_blk.blk_size) + size) + ctx.sp_blk.blk_size - 1) / ctx.sp_blk.blk_size;
    buf_tmp     = mofs_malloc(req_blk_num * ctx.sp_blk.blk_size);
    if (buf_tmp == NULL) {
        ret = get_errno();
        goto out;
    }
    ret = read_file_data_block((*handle)->inode_num, buf_tmp, (*offset) / ctx.sp_blk.blk_size, req_blk_num,
                               &read_blk_num, &fraction);
    if (ret != 0) {
        goto out;
    } else {
        if (((read_blk_num * ctx.sp_blk.blk_size) + fraction) < ((*offset) % ctx.sp_blk.blk_size)) {
            (*read_size) = 0;
        } else {
            (*read_size) = (read_blk_num * ctx.sp_blk.blk_size) + fraction - ((*offset) % ctx.sp_blk.blk_size);
        }
        if ((*read_size) > size) {
            (*read_size) = size;
        }
        mofs_memcpy(buf, (char *)buf_tmp + ((*offset) % ctx.sp_blk.blk_size), (*read_size));
        if (update_offset) {
            (*handle)->file_offset = (unsigned int)(*offset + (off_t)(*read_size));
        }
        if ((*read_size) > 0U) {
            ret = mofs_inode_stamp_now(&inode, MOFS_INODE_TIME_ATIME);
            if (ret == 0) {
                ret = mofs_write_inode((*handle)->inode_num, &inode);
            }
        }
    }

out:
    if (buf_tmp != NULL) {
        mofs_free(buf_tmp);
    }
    return ret;
}

/**
 * @brief Write file data at specified offset in MOFS core layer.
 *
 * Function behavior:
 * - Validates handle state and write arguments.
 * - Verifies write permission from open flags and caller credentials.
 * - Allocates additional file data blocks when write range exceeds EOF.
 * - Performs block-based read-modify-write and updates inode size on extend.
 *
 * @param[in,out] handle Address of opened file handle pointer.
 * @param[in] buf Source buffer containing data to write.
 * @param[in] size Number of bytes requested to write.
 * @param[in,out] offset Byte offset in file to start writing.
 * @param[out] written_size Actual number of bytes written.
 * @param[in] update_offset If true, updates `handle->file_offset` by written bytes.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EBADF if handle is not opened with write access.
 * @return MOFS_EPERM or MOFS_EACCES if caller has no write permission.
 * @return MOFS_EISDIR if target inode is a directory.
 * @return MOFS_EFBIG if requested offset/size exceeds max file size.
 * @return MOFS_ENOSPC if file-size limit is exceeded.
 * @return Other non-zero errno values propagated from lower layers.
 */
int mofs_write_core(mofs_filehandle_t **handle, const void *buf, size_t size, off_t *offset, size_t *written_size,
                    bool update_offset)
{
    int             ret = 0;
    mofs_inode_t    inode;
    mofs_user_ctx_t user;
    void           *buf_tmp          = NULL;
    size_t          fraction         = 0U;
    unsigned int    read_blk_num     = 0U;
    unsigned int    written_blk_num  = 0U;
    unsigned int    start_blk_num    = 0U;
    unsigned int    req_blk_num      = 0U;
    unsigned int    current_blk_num  = 0U;
    unsigned int    required_blk_num = 0U;
    unsigned int    alloc_start_blk  = 0U;
    size_t          write_size_req   = size;
    size_t          total_write_size = 0U;
    size_t          write_pos_in_blk = 0U;
    uint32_t        old_size         = 0U;
    uint64_t        max_file_size    = mofs_max_file_bytes(); /* MOFS_MAX_FILE_DATA_BLOCKS * blk_size */
    uint64_t        write_end_offset = 0U;
    bool            alloc_done       = false;
    bool            write_started    = false;

    if ((handle == NULL) || (*handle == NULL) || ((*handle)->used == false) || (buf == NULL) || (size == 0U) ||
        (offset == NULL) || (*offset < 0) || (written_size == NULL)) {
        ret = MOFS_EINVAL;
        goto out;
    }
    *written_size = 0U;

    /* check handle open flags and permission */
    if (((*handle)->open_flags & MOFS_OFLAG_WRONLY) == 0U) {
        ret = MOFS_EBADF;
        goto out;
    }

    /* read inode */
    ret = mofs_read_inode((*handle)->inode_num, &inode);
    if (ret != 0) {
        goto out;
    }
    old_size = inode.i_size;

    /* check user and permission */
    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        goto out;
    } else if (user.valid == false) {
        ret = MOFS_EPERM;
        goto out;
    }
    ret = mofs_check_open_permission((*handle)->open_flags & MOFS_OFLAG_ACCMODE, &user, &inode);
    if (ret != 0) {
        goto out;
    }

    /* check file type */
    if (inode.i_mode & MOFS_FTYPE_DIR) {
        ret = MOFS_EISDIR;
        goto out;
    }

    if ((uint64_t)(*offset) >= max_file_size) {
        ret = MOFS_EFBIG;
        goto out;
    }
    if (write_size_req > (size_t)(max_file_size - (uint64_t)(*offset))) {
        write_size_req = (size_t)(max_file_size - (uint64_t)(*offset));
    }
    if (write_size_req == 0U) {
        ret = 0;
        goto out;
    }

    start_blk_num    = (unsigned int)((uint64_t)(*offset) / (uint64_t)ctx.sp_blk.blk_size);
    write_pos_in_blk = (size_t)((uint64_t)(*offset) % (uint64_t)ctx.sp_blk.blk_size);
    req_blk_num = (unsigned int)((write_pos_in_blk + write_size_req + ctx.sp_blk.blk_size - 1U) / ctx.sp_blk.blk_size);

    current_blk_num = (old_size + ctx.sp_blk.blk_size - 1U) / ctx.sp_blk.blk_size;
    if (start_blk_num + req_blk_num > current_blk_num) {
        required_blk_num = start_blk_num + req_blk_num - current_blk_num;
        alloc_start_blk  = current_blk_num;
        ret              = allocate_data_block((*handle)->inode_num, required_blk_num);
        if (ret != 0) {
            goto out;
        }
        alloc_done = true;
        ret        = mofs_read_inode((*handle)->inode_num, &inode);
        if (ret != 0) {
            goto out;
        }
    }

    buf_tmp = mofs_malloc((size_t)req_blk_num * ctx.sp_blk.blk_size);
    if (buf_tmp == NULL) {
        ret = get_errno();
        goto out;
    }
    mofs_memset(buf_tmp, 0, (size_t)req_blk_num * ctx.sp_blk.blk_size);

    /* Preload overlapping existing blocks (read-modify-write) via list mapping. */
    current_blk_num = (old_size + ctx.sp_blk.blk_size - 1U) / ctx.sp_blk.blk_size;
    for (unsigned int i = 0U; i < req_blk_num; i++) {
        unsigned int file_blk_num = start_blk_num + i;
        unsigned int abs_blk_num  = 0U;
        if (file_blk_num >= current_blk_num) {
            continue;
        }
        ret = resolve_file_data_block((*handle)->inode_num, file_blk_num, &abs_blk_num);
        if (ret != 0) {
            goto out;
        }
        if ((abs_blk_num < ctx.sp_blk.data_region_start) ||
            (ctx.sp_blk.data_region_start + ctx.sp_blk.data_blk_num <= abs_blk_num)) {
            ret = MOFS_EIO;
            goto out;
        }

        ret = read_continuous_blocks(ctx.dev_fd, (char *)buf_tmp + (size_t)i * ctx.sp_blk.blk_size, 1U, abs_blk_num,
                                     &read_blk_num, &fraction);
        if (ret != 0) {
            goto out;
        } else if ((read_blk_num != 1U) || (fraction != 0U)) {
            ret = MOFS_EIO;
            goto out;
        }
    }

    /* zero-fill the gap between old EOF and write offset if needed */
    if ((uint64_t)old_size < (uint64_t)(*offset)) {
        uint64_t start_off     = (uint64_t)start_blk_num * (uint64_t)ctx.sp_blk.blk_size;
        uint64_t zero_from_off = ((uint64_t)old_size > start_off) ? (uint64_t)old_size : start_off;
        uint64_t zero_to_off   = (uint64_t)(*offset);
        if (zero_to_off > zero_from_off) {
            size_t zero_from_inbuf = (size_t)(zero_from_off - start_off);
            size_t zero_size       = (size_t)(zero_to_off - zero_from_off);
            mofs_memset((char *)buf_tmp + zero_from_inbuf, 0, zero_size);
        }
    }

    mofs_memcpy((char *)buf_tmp + write_pos_in_blk, buf, write_size_req);

    write_started = true;
    ret = write_file_data_block((*handle)->inode_num, buf_tmp, start_blk_num, req_blk_num, &written_blk_num, &fraction);
    if (ret != 0) {
        goto out;
    }

    total_write_size = (size_t)written_blk_num * ctx.sp_blk.blk_size + fraction;
    if (total_write_size < write_pos_in_blk) {
        *written_size = 0U;
    } else {
        *written_size = total_write_size - write_pos_in_blk;
        if (*written_size > write_size_req) {
            *written_size = write_size_req;
        }
    }

    write_end_offset = (uint64_t)(*offset) + (uint64_t)(*written_size);
    if ((ret == 0) && (write_end_offset > (uint64_t)inode.i_size)) {
        inode.i_size = (uint32_t)write_end_offset;
    }
    if ((ret == 0) && ((*written_size) > 0U)) {
        ret = mofs_inode_stamp_now(&inode, MOFS_INODE_TIME_MTIME | MOFS_INODE_TIME_CTIME);
        if (ret == 0) {
            ret = mofs_write_inode((*handle)->inode_num, &inode);
        }
    }

    if (update_offset) {
        (*handle)->file_offset = (unsigned int)((uint64_t)(*offset) + (uint64_t)(*written_size));
    }

out:
    if ((ret != 0) && (alloc_done == true) && (write_started == false) && (handle != NULL) && (*handle != NULL)) {
        (void)free_data_block((*handle)->inode_num, alloc_start_blk, required_blk_num);
    }
    if (buf_tmp != NULL) {
        mofs_free(buf_tmp);
    }
    return ret;
}

/**
 * @brief Fill the tail bytes of one data block with zeros on shrink truncate.
 *
 * Function behavior:
 * - Resolves the absolute block number for the given file-relative block index.
 * - Reads one full block, zero-fills from `tail_start` to block end, and writes it back.
 * - Used when truncate leaves a partial last block that must not expose stale data.
 *
 * @param[in] inode_num Target file inode number.
 * @param[in] blk_idx File-relative data block index to modify.
 * @param[in] tail_start Byte offset within the block from which to zero.
 * @return 0 on success.
 * @return MOFS_EINVAL if `tail_start` is out of block range.
 * @return MOFS_EIO if block read/write does not complete as expected.
 * @return Other non-zero errno values propagated from lower layers.
 */
static int zero_partial_block_tail(int inode_num, unsigned int blk_idx, size_t tail_start)
{
    int          ret              = 0;
    void        *buf              = NULL;
    unsigned int abs_blk_num      = 0U;
    unsigned int read_blk_num     = 0U;
    unsigned int written_blk_num  = 0U;
    size_t       fraction         = 0U;
    size_t       written_fraction = 0U;

    if (tail_start >= ctx.sp_blk.blk_size) {
        return MOFS_EINVAL;
    }

    buf = mofs_malloc(ctx.sp_blk.blk_size);
    if (buf == NULL) {
        return get_errno();
    }

    /* Logical block index -> absolute data block via on-disk list nodes. */
    ret = resolve_file_data_block(inode_num, blk_idx, &abs_blk_num);
    if (ret != 0) {
        goto out;
    }

    ret = read_continuous_blocks(ctx.dev_fd, buf, 1U, abs_blk_num, &read_blk_num, &fraction);
    if ((ret != 0) || (read_blk_num != 1U) || (fraction != 0U)) {
        ret = MOFS_EIO;
        goto out;
    }

    /* Discard bytes beyond the new EOF inside the retained last block. */
    mofs_memset((char *)buf + tail_start, 0, ctx.sp_blk.blk_size - tail_start);

    ret = write_continuous_blocks(ctx.dev_fd, buf, 1U, abs_blk_num, &written_blk_num, &written_fraction);
    if ((ret != 0) || (written_blk_num != 1U) || (written_fraction != 0U)) {
        ret = MOFS_EIO;
    }

out:
    if (buf != NULL) {
        mofs_free(buf);
    }
    return ret;
}

/**
 * @brief Fill a byte range with zeros inside an already extended and mapped as a file.
 *
 * Function behavior:
 * - Covers `[from_off, to_off)` with block-based read-modify-write.
 * - Preloads existing block contents for overlapping regions before zeroing.
 * - Writes back all blocks spanned by the requested range.
 * - Used when truncate grows a file so the extended area reads as zero-filled.
 *
 * @param[in] inode_num Target file inode number.
 * @param[in] from_off Start byte offset (inclusive).
 * @param[in] to_off End byte offset (exclusive).
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EIO if block read/write does not complete as expected.
 * @return Other non-zero errno values propagated from lower layers.
 */
static int zero_file_byte_range(int inode_num, uint32_t from_off, uint32_t to_off)
{
    int          ret = 0;
    mofs_inode_t inode;
    void        *buf_tmp         = NULL;
    size_t       fraction        = 0U;
    unsigned int read_blk_num    = 0U;
    unsigned int written_blk_num = 0U;
    unsigned int start_blk_num   = 0U;
    unsigned int req_blk_num     = 0U;
    unsigned int current_blk_num = 0U;
    uint32_t     old_size        = 0U;

    if (from_off >= to_off) {
        return 0;
    }

    ret = mofs_read_inode(inode_num, &inode);
    if (ret != 0) {
        return ret;
    }
    old_size = inode.i_size;

    /* Compute the block span that covers [from_off, to_off). */
    start_blk_num = from_off / ctx.sp_blk.blk_size;
    req_blk_num   = ((to_off - 1U) / ctx.sp_blk.blk_size) - start_blk_num + 1U;

    buf_tmp = mofs_malloc((size_t)req_blk_num * ctx.sp_blk.blk_size);
    if (buf_tmp == NULL) {
        return get_errno();
    }
    mofs_memset(buf_tmp, 0, (size_t)req_blk_num * ctx.sp_blk.blk_size);

    /* Preload overlapping existing blocks (read-modify-write) via list mapping. */
    current_blk_num = (old_size + ctx.sp_blk.blk_size - 1U) / ctx.sp_blk.blk_size;
    for (unsigned int i = 0U; i < req_blk_num; i++) {
        unsigned int file_blk_num = start_blk_num + i;
        unsigned int abs_blk_num  = 0U;

        if (file_blk_num >= current_blk_num) {
            continue;
        }
        ret = resolve_file_data_block(inode_num, file_blk_num, &abs_blk_num);
        if (ret != 0) {
            goto out;
        }
        ret = read_continuous_blocks(ctx.dev_fd, (char *)buf_tmp + (size_t)i * ctx.sp_blk.blk_size, 1U, abs_blk_num,
                                     &read_blk_num, &fraction);
        if ((ret != 0) || (read_blk_num != 1U) || (fraction != 0U)) {
            ret = MOFS_EIO;
            goto out;
        }
    }

    /* Zero only the target sub-range within the assembled block buffer. */
    {
        uint64_t start_off     = (uint64_t)start_blk_num * (uint64_t)ctx.sp_blk.blk_size;
        uint64_t zero_from_off = ((uint64_t)from_off > start_off) ? (uint64_t)from_off : start_off;
        uint64_t zero_to_off   = (uint64_t)to_off;
        if (zero_to_off > zero_from_off) {
            size_t zero_from_inbuf = (size_t)(zero_from_off - start_off);
            size_t zero_size       = (size_t)(zero_to_off - zero_from_off);
            mofs_memset((char *)buf_tmp + zero_from_inbuf, 0, zero_size);
        }
    }

    ret = write_file_data_block(inode_num, buf_tmp, start_blk_num, req_blk_num, &written_blk_num, &fraction);

out:
    if (buf_tmp != NULL) {
        mofs_free(buf_tmp);
    }
    return ret;
}

/**
 * @brief Apply truncate semantics to one regular-file inode.
 *
 * Function behavior:
 * - Validates inode type and caller write permission.
 * - Shrinks by zeroing a partial tail block, freeing trailing blocks, and updating `i_size`.
 * - Grows by allocating additional blocks, zero-filling the extended range, and updating `i_size`.
 * - Rolls back newly allocated blocks if a grow operation fails after allocation.
 *
 * @param[in] inode_num Target regular-file inode number.
 * @param[in] new_size New file size in bytes.
 * @return 0 on success.
 * @return MOFS_EISDIR if target inode is a directory.
 * @return MOFS_EPERM or MOFS_EACCES if caller has no write permission.
 * @return Other non-zero errno values propagated from lower layers.
 */
static int truncate_inode(int inode_num, uint32_t new_size)
{
    int             ret = 0;
    mofs_inode_t    inode;
    mofs_user_ctx_t user;
    uint32_t        old_size         = 0U;
    unsigned int    old_nr_blocks    = 0U;
    unsigned int    keep_blks        = 0U;
    unsigned int    needed_blks      = 0U;
    unsigned int    alloc_start_blk  = 0U;
    unsigned int    required_blk_num = 0U;
    bool            alloc_done       = false;

    ret = mofs_read_inode(inode_num, &inode);
    if (ret != 0) {
        return ret;
    }

    if ((inode.i_mode & MOFS_FTYPE_DIR) != 0U) {
        return MOFS_EISDIR;
    }

    ret = mofs_get_caller_user(&user);
    if (ret != 0) {
        return ret;
    } else if (user.valid == false) {
        return MOFS_EPERM;
    }
    ret = mofs_check_open_permission(MOFS_OFLAG_WRONLY, &user, &inode);
    if (ret != 0) {
        return ret;
    }

    old_size      = inode.i_size;
    old_nr_blocks = inode.i_nr_blocks;
    if (new_size == old_size) {
        return 0;
    }

    if (new_size < old_size) {
        /* Shrink: retain ceil(new_size / blk_size) data blocks at the front. */
        keep_blks = (new_size == 0U) ? 0U : (new_size + ctx.sp_blk.blk_size - 1U) / ctx.sp_blk.blk_size;

        if ((new_size > 0U) && ((new_size % ctx.sp_blk.blk_size) != 0U) && (keep_blks > 0U)) {
            ret = zero_partial_block_tail(inode_num, keep_blks - 1U, new_size % ctx.sp_blk.blk_size);
        }
        if ((ret == 0) && (keep_blks < old_nr_blocks)) {
            /* Release file data blocks beyond the retained prefix. */
            ret = free_data_block(inode_num, keep_blks, old_nr_blocks - keep_blks);
        }
        if (ret == 0) {
            ret = mofs_read_inode(inode_num, &inode);
        }
        if (ret == 0) {
            inode.i_size = new_size;
            ret          = mofs_inode_stamp_now(&inode, MOFS_INODE_TIME_MTIME | MOFS_INODE_TIME_CTIME);
            if (ret == 0) {
                ret = mofs_write_inode(inode_num, &inode);
            }
        }
    } else {
        /* Grow: allocate blocks if needed, then zero-fill [old_size, new_size). */
        needed_blks = (new_size + ctx.sp_blk.blk_size - 1U) / ctx.sp_blk.blk_size;
        if (needed_blks > inode.i_nr_blocks) {
            required_blk_num = needed_blks - inode.i_nr_blocks;
            alloc_start_blk  = inode.i_nr_blocks;
            ret              = allocate_data_block(inode_num, required_blk_num);
            if (ret != 0) {
                return ret;
            }
            alloc_done = true;
            ret        = mofs_read_inode(inode_num, &inode);
            if (ret != 0) {
                goto rollback;
            }
        }

        ret = zero_file_byte_range(inode_num, old_size, new_size);
        if (ret != 0) {
            goto rollback;
        }

        inode.i_size = new_size;
        ret          = mofs_inode_stamp_now(&inode, MOFS_INODE_TIME_MTIME | MOFS_INODE_TIME_CTIME);
        if (ret == 0) {
            ret = mofs_write_inode(inode_num, &inode);
        }
        if (ret != 0) {
            goto rollback;
        }
    }

    return ret;

rollback:
    if (alloc_done) {
        (void)free_data_block(inode_num, alloc_start_blk, required_blk_num);
    }
    return ret;
}

/**
 * @brief Truncate a regular file to the specified length in MOFS core layer.
 *
 * Function behavior:
 * - Validates inode number and target length against max file size.
 * - Rejects directory inodes and verifies caller write permission.
 * - Shrinks by freeing trailing data blocks and zeroing partial tail bytes.
 * - Grows by allocating blocks and zero-filling the extended range.
 * - Updates inode `i_size` on success.
 *
 * @param[in] inode_num Target regular-file inode number.
 * @param[in] length New file size in bytes.
 * @return 0 on success.
 * @return MOFS_EINVAL if arguments are invalid.
 * @return MOFS_EFBIG if length exceeds max file size.
 * @return MOFS_EISDIR if target inode is a directory.
 * @return MOFS_EPERM or MOFS_EACCES if caller has no write permission.
 * @return Other non-zero errno values propagated from lower layers.
 */
int mofs_truncate_core(int inode_num, off_t length)
{
    /* Reject invalid inode and negative length before touching on-disk metadata. */
    if (inode_num < 0) {
        return MOFS_EINVAL;
    }
    if (length < 0) {
        return MOFS_EINVAL;
    }
    if ((uint64_t)length > mofs_max_file_bytes()) {
        return MOFS_EFBIG;
    }

    return truncate_inode(inode_num, (uint32_t)length);
}

/**
 * @brief Resolve a path and read its inode metadata.
 *
 * Function behavior:
 * - Validates input pointers.
 * - Resolves inode number from the specified path.
 * - Reads inode contents for the resolved inode number.
 *
 * @param[in] path NULL-terminated absolute path string.
 * @param[out] stbuf Destination pointer for resolved inode metadata.
 * @return 0 on success.
 * @return MOFS_EINVAL if any argument is invalid.
 * @return Non-zero error returned by `mofs_path_to_inode_num()` or `mofs_read_inode()`
 *         when lookup/read fails.
 */
int mofs_stat_core(const char *path, mofs_stat_t *stbuf)
{
    int             ret       = 0;
    int             inode_num = -1;
    mofs_inode_t    inode;
    mofs_user_ctx_t user;

    if ((path == NULL) || (stbuf == NULL)) {
        ret = MOFS_EINVAL;
    }

    if (ret == 0) {
        mofs_memset(stbuf, 0, sizeof(mofs_stat_t));
        mofs_memset(&inode, 0, sizeof(mofs_inode_t));

        ret = mofs_get_caller_user(&user);
        if (ret == 0) {
            if (user.valid == false) {
                ret = MOFS_EPERM;
            }
        }
    }

    if (ret == 0) {
        ret = mofs_path_to_inode_num(path, &inode_num);
        if (ret == 0) {
            ret = mofs_read_inode(inode_num, &inode);
            if (ret == 0) {
                if ((inode.i_mode & MOFS_FTYPE_DIR) != 0U) {
                    ret = mofs_check_dir_traverse(&user, &inode);
                } else {
                    ret = mofs_check_open_permission(MOFS_OFLAG_RDONLY, &user, &inode);
                }
            }
        }
    }

    if (ret == 0) {
        stbuf->st_ino       = inode_num;
        stbuf->st_nlink     = inode.i_links;
        stbuf->st_size      = inode.i_size;
        stbuf->st_mode      = inode.i_mode;
        stbuf->st_uid       = inode.i_uid;
        stbuf->st_gid       = inode.i_gid;
        stbuf->st_atime_sec = (int64_t)inode.i_atime;
        stbuf->st_mtime_sec = (int64_t)inode.i_mtime;
        stbuf->st_ctime_sec = (int64_t)inode.i_ctime;
    }

    return ret;
}
