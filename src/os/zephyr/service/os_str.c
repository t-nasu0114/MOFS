#include <mofs_port_str.h>

/* Zephyr stub: minimal string helpers (no libc dependency for now). */

int mofs_strcmp(const char *s1, const char *s2)
{
    if ((s1 == NULL) || (s2 == NULL)) {
        if (s1 == s2) {
            return 0;
        }
        return (s1 == NULL) ? -1 : 1;
    }

    while ((*s1 != '\0') && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1) - (int)((unsigned char)*s2);
}

char *mofs_strcpy(char *dest, const char *src)
{
    char *out = dest;

    if ((dest == NULL) || (src == NULL)) {
        return dest;
    }

    while (*src != '\0') {
        *dest++ = *src++;
    }
    *dest = '\0';
    return out;
}

mofs_size_t mofs_strlen(const char *s)
{
    mofs_size_t n = 0;

    if (s == NULL) {
        return 0;
    }

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

char *mofs_strtok(char *str, const char *delim)
{
    (void)str;
    (void)delim;
    return NULL;
}
