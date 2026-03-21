#ifndef __MOFS_STR__
#define __MOFS_STR__

#include <mofs_type.h>

int    mofs_strcmp(const char *s1, const char *s2);
char  *mofs_strcpy(char *dest, const char *src);
size_t mofs_strlen(const char *s);
char  *mofs_strtok(char *str, const char *delim);

#endif /* __MOFS_STR__ */
