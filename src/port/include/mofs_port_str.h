#ifndef __MOFS_PORT_STR__
#define __MOFS_PORT_STR__

#include <mofs_port_types.h>

/**
 * @file mofs_port_str.h
 * @brief Contract for null-terminated string operations used by MOFS.
 */

/**
 * @brief Compare two null-terminated strings lexicographically.
 *
 * @param[in] s1 First string.
 * @param[in] s2 Second string.
 * @return A value less than, equal to, or greater than zero when `s1` is
 *         respectively less than, equal to, or greater than `s2`.
 */
int mofs_strcmp(const char *s1, const char *s2);

/**
 * @brief Copy a null-terminated string.
 *
 * @param[out] dest Destination with enough space for `src` and its terminator.
 * @param[in] src Source string.
 * @return `dest`.
 *
 * @note Source and destination must not overlap.
 */
char *mofs_strcpy(char *dest, const char *src);

/**
 * @brief Return the length of a null-terminated string.
 *
 * @param[in] s String to measure.
 * @return Number of bytes before the terminating null character.
 */
mofs_size_t mofs_strlen(const char *s);

/**
 * @brief Split a string into tokens using delimiter characters.
 *
 * Function behavior:
 * - Pass a non-NULL `str` to begin tokenizing a mutable string.
 * - Pass NULL on subsequent calls to continue the same token sequence.
 * - Replaces delimiter bytes in `str` with null terminators.
 *
 * @param[in,out] str String to tokenize, or NULL to continue.
 * @param[in] delim Null-terminated set of delimiter characters.
 * @return Pointer to the next token.
 * @return NULL when no tokens remain.
 *
 * @note The continuation state is implementation-owned and need not be
 *       reentrant. Callers must externally serialize concurrent tokenization.
 */
char *mofs_strtok(char *str, const char *delim);

#endif /* __MOFS_PORT_STR__ */
