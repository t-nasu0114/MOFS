# MOFS on-disk layout (data block list)

## Inode (`mofs_inode_t`, 64 bytes)

| Field | Description |
|-------|-------------|
| `i_size` | File size in bytes |
| `i_links` | Link count |
| `i_mode` | Type and permissions |
| `i_uid` / `i_gid` | Owner |
| `i_data_head` | Absolute block number of the first **list node** (0 = no mapping) |
| `i_nr_blocks` | Number of **file data** blocks (list nodes not included) |
| `i_atime` | Last access time (Unix epoch seconds) |
| `i_mtime` | Last modification time (Unix epoch seconds) |
| `i_ctime` | Last status change time (Unix epoch seconds) |
| `reserved[4]` | Padding |

## Data block list node (one logical block)

| Offset | Field |
|--------|--------|
| 0 | `next_abs` — next list node absolute block, or 0 |
| 4 | `nr_ptrs` — count of valid pointers in this node |
| 8+ | `uint32_t` absolute block numbers for file data |

Pointers per node: `(blk_size - 8) / 4` (power-of-two `blk_size`).

## File size limit

- Max data blocks per file: `MOFS_MAX_FILE_DATA_BLOCKS` (default 1024, compile-time).
- Max bytes: `MOFS_MAX_FILE_DATA_BLOCKS * blk_size` (see `mofs_max_file_bytes()` after mount).

## Migration

There is no on-disk version field. After this layout change, **re-run mkfs** on existing images.
