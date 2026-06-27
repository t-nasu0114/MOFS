#ifndef __MOFS_CONFIG__
#define __MOFS_CONFIG__

/*******************************************************
 * Build-time feature configuration
 *
 * The macros below provide default values only. The build
 * system (for example, CMake `target_compile_definitions`)
 * may override them; the `#ifndef` guards let an externally
 * supplied value take precedence over these defaults.
 *******************************************************/

/* Block buffer cache feature toggle (1: enabled, 0: disabled). */
#ifndef MOFS_BUFFER_CACHE_ENABLE
#define MOFS_BUFFER_CACHE_ENABLE 1
#endif

/* Number of block buffers held by the buffer cache pool. */
#ifndef MOFS_BUFFER_CACHE_NUM
#define MOFS_BUFFER_CACHE_NUM 64U
#endif

/* Split-cache pool sizes (used when MOFS_BCACHE_SPLIT=1).
 * Meta pool covers all blocks below data_region_start (superblock, bitmaps,
 * inode table). Data pool covers file data and list-node blocks. */
#ifndef MOFS_META_CACHE_NUM
#define MOFS_META_CACHE_NUM 16U
#endif
#ifndef MOFS_DATA_CACHE_NUM
#define MOFS_DATA_CACHE_NUM 48U
#endif

#endif /* __MOFS_CONFIG__ */
