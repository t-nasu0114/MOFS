#ifndef __MOFS_TIME__
#define __MOFS_TIME__

#include <mofs_type.h>

typedef uint64_t mofs_time_sec_t;

#define MOFS_TIME_INVALID 0ULL

int mofs_now(mofs_time_sec_t *now);

#endif /* __MOFS_TIME__ */
