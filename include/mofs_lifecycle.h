#ifndef __MOFS_LIFECYCLE__
#define __MOFS_LIFECYCLE__

#include <mofs_type.h>

int mofs_init_core(const char *path, bool update_root_owner, uint32_t root_uid, uint32_t root_gid);
int mofs_fini_core(void);

#endif /* __MOFS_LIFECYCLE__ */
