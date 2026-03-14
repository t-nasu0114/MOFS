#include <mofs_mem.h>
#include <stdlib.h>
#include <string.h>

void *mofs_malloc(size_t size)
{
    return malloc(size);
}

void mofs_free(void *ptr)
{
    free(ptr);
}

void *mofs_memcpy(void *dest, const void *src, size_t n)
{
    return memcpy(dest, src, n);
}
