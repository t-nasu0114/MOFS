#ifndef __MOFS_OS_TYPES__
#define __MOFS_OS_TYPES__

#include <stdint.h>

typedef uint8_t  mofs_uint8_t;
typedef uint16_t mofs_uint16_t;
typedef uint32_t mofs_uint32_t;
typedef uint64_t mofs_uint64_t;

typedef int8_t  mofs_int8_t;
typedef int16_t mofs_int16_t;
typedef int32_t mofs_int32_t;
typedef int64_t mofs_int64_t;

typedef mofs_uint32_t mofs_size_t;
typedef mofs_int64_t  mofs_off_t;
typedef mofs_uint32_t mofs_mode_t;
typedef mofs_uint32_t mofs_uid_t;
typedef mofs_uint32_t mofs_gid_t;
typedef mofs_int32_t  mofs_pid_t;
typedef mofs_uint32_t mofs_ino_t;
typedef mofs_uint16_t mofs_nlink_t;

typedef mofs_uint32_t mofs_bool;
#define MOFS_TRUE  1U
#define MOFS_FALSE 0U

#ifndef NULL
#define NULL ((void *)0)
#endif

#define MOFS_UINT32_MAX UINT32_MAX
#define MOFS_UINT64_MAX UINT64_MAX

typedef char mofs_os_type_check_uint8[(sizeof(mofs_uint8_t) == 1U) ? 1 : -1];
typedef char mofs_os_type_check_uint16[(sizeof(mofs_uint16_t) == 2U) ? 1 : -1];
typedef char mofs_os_type_check_uint32[(sizeof(mofs_uint32_t) == 4U) ? 1 : -1];
typedef char mofs_os_type_check_uint64[(sizeof(mofs_uint64_t) == 8U) ? 1 : -1];
typedef char mofs_os_type_check_int32[(sizeof(mofs_int32_t) == 4U) ? 1 : -1];
typedef char mofs_os_type_check_int64[(sizeof(mofs_int64_t) == 8U) ? 1 : -1];

#endif /* __MOFS_OS_TYPES__ */
