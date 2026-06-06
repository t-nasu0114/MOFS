#ifndef __MOFS_PORT_MEM__
#define __MOFS_PORT_MEM__

#include <mofs_port_types.h>

void *mofs_malloc(mofs_size_t size);
void  mofs_free(void *ptr);
void *mofs_memcpy(void *dest, const void *src, mofs_size_t n);
void *mofs_memset(void *s, int c, mofs_size_t n);

#endif /* __MOFS_PORT_MEM__ */
