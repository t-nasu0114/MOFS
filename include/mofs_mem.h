#ifndef __MOFS_MEM__
#define __MOFS_MEM__

#include <stddef.h>

void *mofs_malloc(size_t size);
void  mofs_free(void *ptr);

#endif /* __MOFS_MEM__ */