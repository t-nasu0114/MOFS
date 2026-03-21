#ifndef __MOFS_MEM__
#define __MOFS_MEM__

#include <stddef.h>

void *mofs_malloc(size_t size);
void  mofs_free(void *ptr);
void *mofs_memcpy(void *dest, const void *src, size_t n);
void *mofs_memset(void *s, int c, size_t n);

#endif /* __MOFS_MEM__ */