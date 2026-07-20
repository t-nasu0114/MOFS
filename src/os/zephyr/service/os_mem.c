#include <mofs_port_mem.h>
#include <string.h>
#include <zephyr/kernel.h>

void *mofs_malloc(mofs_size_t size)
{
    return k_malloc((size_t)size);
}

void mofs_free(void *ptr)
{
    k_free(ptr);
}

void *mofs_memcpy(void *dest, const void *src, mofs_size_t n)
{
    return memcpy(dest, src, (size_t)n);
}

void *mofs_memset(void *s, int c, mofs_size_t n)
{
    return memset(s, c, (size_t)n);
}
