#ifndef __MOFS_PORT_SYNC__
#define __MOFS_PORT_SYNC__

#include <mofs_port_types.h>

typedef struct mofs_mutex mofs_mutex_t;

int  mofs_mutex_init(mofs_mutex_t **mutex);
void mofs_mutex_fini(mofs_mutex_t *mutex);
int  mofs_mutex_lock(mofs_mutex_t *mutex);
int  mofs_mutex_unlock(mofs_mutex_t *mutex);

/* Serialize core filesystem operations (POSIX / FUSE entry points and inode RMW). */
int  mofs_core_sync_init(void);
void mofs_core_sync_fini(void);
void mofs_core_sync_lock(void);
void mofs_core_sync_unlock(void);

#endif /* __MOFS_PORT_SYNC__ */
