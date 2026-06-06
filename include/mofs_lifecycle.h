#ifndef __MOFS_LIFECYCLE__
#define __MOFS_LIFECYCLE__

#include <mofs_types.h>

int mofs_init_core(const char *path, mofs_bool update_root_owner, mofs_uint32_t root_uid, mofs_uint32_t root_gid);
int mofs_fini_core(void);

#endif /* __MOFS_LIFECYCLE__ */
