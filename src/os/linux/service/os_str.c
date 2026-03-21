#include <string.h>

int mofs_strcmp(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

char *mofs_strcpy(char *dest, const char *src)
{
    return strcpy(dest, src);
}

size_t mofs_strlen(const char *s)
{
    return strlen(s);
}

char *mofs_strtok(char *str, const char *delim)
{
    return strtok(str, delim);
}
