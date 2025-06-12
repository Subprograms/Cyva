/* ============================================================
   Helper.h
   ============================================================ */
   
#ifndef CYVA_HELPER
#define CYVA_HELPER

#include <stddef.h>   /* size_t */

/* ============================================================
   1.  Safe core string helpers  ( *_cap variants )
   ============================================================ */

static inline int cyvaStrcpy_cap(char *dst, size_t dstCap,
                                 const char *src)
{
    if (!dst || !src || dstCap == 0) return -1;
    size_t i = 0;
    while (i + 1 < dstCap && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
    return (src[i] == '\0') ? 0 : -1;   /* 0 = success, -1 = truncated */
}

static inline int cyvaStrcat_cap(char *dst, size_t dstCap,
                                 const char *src)
{
    if (!dst || !src || dstCap == 0) return -1;
    size_t i = 0; while (i < dstCap && dst[i]) ++i;
    if (i == dstCap) { dst[dstCap - 1] = '\0'; return -1; }
    size_t j = 0;
    while (i + 1 < dstCap && src[j]) { dst[i++] = src[j++]; }
    dst[i] = '\0';
    return (src[j] == '\0') ? 0 : -1;
}

static inline int cyvaStrncpy_cap(char *dst, size_t dstCap,
                                  const char *src, size_t n)
{
    if (!dst || !src || dstCap == 0) return -1;
    size_t i = 0;
    while (i < n && i + 1 < dstCap && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
    while (i < n && i + 1 < dstCap) dst[++i] = '\0';
    return (src[i] == '\0') ? 0 : -1;
}

/* ============================================================
   2.  Back-compat wrappers
   ------------------------------------------------------------
   Existing CYVA code uses 2- and 3-argument forms.  The macros
   below forward those calls to the new *_cap versions, passing
   sizeof(dst) when ‘dst’ is a true array.  If ‘dst’ is a naked
   char* pointer, sizeof(dst) is only the pointer size — the copy
   will truncate, alerting you to fix the call.
   ============================================================ */

#define cyvaStrcpy(dst, src)         cyvaStrcpy_cap((dst), sizeof(dst), (src))
#define cyvaStrcat(dst, src)         cyvaStrcat_cap((dst), sizeof(dst), (src))
#define cyvaStrncpy(dst, src, n)     cyvaStrncpy_cap((dst), sizeof(dst), (src), (n))

/* ============================================================
   3.  Tiny string primitives (unchanged originals)
   ============================================================ */

static inline size_t cyvaStrlen(const char *s)
{
    size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

static inline char *cyvaStrchr(const char *s, int ch)
{
    if (!s) return NULL;
    while (*s && *s != (char)ch) ++s;
    return (*s == (char)ch) ? (char *)s : NULL;
}

static inline int cyvaStrcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static inline int cyvaStrncmp(const char *a,
                              const char *b,
                              size_t n)
{
    while (n && *a && (*a == *b)) { ++a; ++b; --n; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static inline char *cyvaStrstr(const char *hay, const char *needle)
{
    if (!hay || !needle || !*needle) return (char *)hay;
    size_t nl = cyvaStrlen(needle);
    for (; *hay; ++hay)
        if (*hay == *needle && cyvaStrncmp(hay, needle, nl) == 0)
            return (char *)hay;
    return NULL;
}

static inline char *cyvaStrpbrk(const char *s, const char *accept)
{
    if (!s || !accept) return NULL;
    for (; *s; ++s)
        if (cyvaStrchr(accept, *s))
            return (char *)s;
    return NULL;
}

static inline size_t cyvaStrcspn(const char *s, const char *reject)
{
    size_t n = 0;
    while (s && s[n] && !cyvaStrchr(reject, s[n])) ++n;
    return n;
}

/* ============================================================
   4.  Tokeniser & character helpers
   ============================================================ */

static inline char *cyvaStrtok(char *str, const char *delim)
{
    static char *cyvaTokSave = NULL;

    char *s = str ? str : cyvaTokSave;
    if (!s) return NULL;

    /* Skip leading delimiters */
    while (*s && cyvaStrchr(delim, *s)) ++s;
    if (!*s) { cyvaTokSave = NULL; return NULL; }

    char *tok = s;
    while (*s && !cyvaStrchr(delim, *s)) ++s;

    if (*s) {
        *s = '\0';
        cyvaTokSave = s + 1;
    } else {
        cyvaTokSave = NULL;
    }
    return tok;
}

static inline int cyvaIsalpha(int ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

/* ============================================================
   5.  Minimal memcpy
   ============================================================ */
static inline void *cyvaMemcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* ============================================================
   6.  Tiny decimal-only strtol
   ============================================================ */
static inline long cyvaStrtol(const char *s, char **end, int base)
{
    (void)base;                       /* CYVA uses base-10 only */
    if (!s) { if (end) *end = NULL; return 0; }

    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\f' || *s == '\v') ++s;

    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; ++s; }

    long val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10L + (*s - '0');
        ++s;
    }

    if (end) *end = (char *)s;
    return val * sign;
}

#endif
