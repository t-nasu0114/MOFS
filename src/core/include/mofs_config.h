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

#endif /* __MOFS_CONFIG__ */
