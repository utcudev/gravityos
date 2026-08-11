/* =============================================================================
 * GravityOS — String Library Implementation
 * ============================================================================= */

#include "string.h"

/* ==========================================================================
 * Bellek İşlemleri
 * ========================================================================== */

void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while(n--) {
        if( *p1 != *p2 ) {
            return *p1 - *p2;
        } else {
            p1++;
            p2++;
        }
    }
    return 0;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

/* ==========================================================================
 * String İşlemleri
 * ========================================================================== */

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

char *strcpy(char *dest, const char *src)
{
    char *ret = dest;
    while ((*dest++ = *src++))
        ;
    return ret;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *ret = dest;
    dest += strlen(dest);
    while ((*dest++ = *src++))
        ;
    return ret;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (c == '\0') ? (char *)s : (char *)last;
}

/* strtok durumu */
static char *strtok_save = NULL;

char *strtok(char *str, const char *delim)
{
    if (str) {
        strtok_save = str;
    }
    if (!strtok_save) {
        return NULL;
    }

    /* Başlangıçtaki delimiter'ları atla */
    char *start = strtok_save;
    while (*start) {
        int is_delim = 0;
        for (const char *d = delim; *d; d++) {
            if (*start == *d) {
                is_delim = 1;
                break;
            }
        }
        if (!is_delim) break;
        start++;
    }

    if (*start == '\0') {
        strtok_save = NULL;
        return NULL;
    }

    /* Token sonunu bul */
    char *end = start;
    while (*end) {
        for (const char *d = delim; *d; d++) {
            if (*end == *d) {
                *end = '\0';
                strtok_save = end + 1;
                return start;
            }
        }
        end++;
    }

    strtok_save = NULL;
    return start;
}

/* ==========================================================================
 * Dönüşüm Fonksiyonları
 * ========================================================================== */

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    /* Boşlukları atla */
    while (*str == ' ' || *str == '\t') str++;

    /* İşaret kontrolü */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

void itoa(int value, char *str, int base)
{
    char *p = str;
    char *p1, *p2;
    int negative = 0;

    if (value < 0 && base == 10) {
        negative = 1;
        value = -value;
    }

    /* Rakamları ters sırada yaz */
    do {
        int digit = value % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    } while (value);

    if (negative) {
        *p++ = '-';
    }
    *p = '\0';

    /* String'i ters çevir */
    p1 = str;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

void utoa(uint64_t value, char *str, int base)
{
    char *p = str;
    char *p1, *p2;

    do {
        uint64_t digit = value % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    } while (value);

    *p = '\0';

    /* String'i ters çevir */
    p1 = str;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}
