#include <mofs_port_mem.h>

/* Zephyr stub: no heap yet (later: k_malloc / k_free). */

void *mofs_malloc(mofs_size_t size)
{
    (void)size;
    return NULL;
}

void mofs_free(void *ptr)
{
    (void)ptr;
}

void *mofs_memcpy(void *dest, const void *src, mofs_size_t n)
{
    mofs_size_t       i;
    unsigned char       *d;
    const unsigned char *s;

    if ((dest == NULL) || (src == NULL)) {
        return dest;
    }

    d = (unsigned char *)dest;
    s = (const unsigned char *)src;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *mofs_memset(void *s, int c, mofs_size_t n)
{
    mofs_size_t   i;
    unsigned char *p;

    if (s == NULL) {
        return s;
    }

    p = (unsigned char *)s;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}
