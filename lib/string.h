/* =============================================================================
 * GravityOS — String Library Header
 * ============================================================================= */

#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stdint.h>
#include <stddef.h>

/* Bellek işlemleri */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);

/* String işlemleri */
size_t strlen(const char *s);
char  *strstr(const char *haystack, const char *needle);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
char  *strcat(char *dest, const char *src);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strtok(char *str, const char *delim);

/* Dönüşüm */
int    atoi(const char *str);
void   itoa(int value, char *str, int base);
void   utoa(uint64_t value, char *str, int base);

#endif /* LIB_STRING_H */
