#ifndef __MOFS_PORT_TIME__
#define __MOFS_PORT_TIME__

#include <mofs_port_types.h>

typedef mofs_uint64_t mofs_time_sec_t;

#define MOFS_TIME_INVALID 0ULL

int mofs_now(mofs_time_sec_t *now);

#endif /* __MOFS_PORT_TIME__ */
