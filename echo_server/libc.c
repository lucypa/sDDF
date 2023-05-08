#include <stddef.h>
#include <stdint.h>

/* The following function implementations are taken from musl libc. */

static uint64_t seed;

int isspace(int c)
{
    return c == ' ' || (unsigned)c - '\t' < 5;
}

int isdigit(int c)
{
    return (unsigned)c - '0' < 10;
}

int atoi(const char *s)
{
    int n = 0, neg=0;
    while (isspace(*s)) s++;
    switch (*s) {
        case '-': neg=1;
        case '+': s++;
    }
    /* Compute n as a negative number to avoid overflow on INT_MIN */
    while (isdigit(*s)) {
        n = 10*n - (*s++ - '0');
    }

    return neg ? n : -n;
}

int rand(void)
{
    seed = 6364136223846793005ULL*seed + 1;
    return seed >> 33;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (; n; n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;

    if (d == s) return d;
    if ((uintptr_t)s - (uintptr_t)d - n <= -2 * n) return memcpy(d, s, n);

    if (d<s) {
        for (; n; n--) *d++ = *s++;
    } else {
        while (n) n--, d[n] = s[n];
    }

    return dest;
}

void *memset(void *dest, int c, size_t n)
{
    unsigned char *s = dest;
    size_t k;

    /* Fill head and tail with minimal branching. Each
     * conditional ensures that all the subsequently used
     * offsets are well-defined and in the dest region. */

    if (!n) return dest;
    s[0] = c;
    s[n-1] = c;
    if (n <= 2) return dest;
    s[1] = c;
    s[2] = c;
    s[n-2] = c;
    s[n-3] = c;
    if (n <= 6) return dest;
    s[3] = c;
    s[n-4] = c;
    if (n <= 8) return dest;

    /* Advance pointer to align it at a 4-byte boundary,
     * and truncate n to a multiple of 4. The previous code
     * already took care of any head/tail that get cut off
     * by the alignment. */

    k = -(uintptr_t)s & 3;
    s += k;
    n -= k;
    n &= -4;

#ifdef __GNUC__
    typedef uint32_t __attribute__((__may_alias__)) u32;
    typedef uint64_t __attribute__((__may_alias__)) u64;

    u32 c32 = ((u32)-1)/255 * (unsigned char)c;

    /* In preparation to copy 32 bytes at a time, aligned on
     * an 8-byte bounary, fill head/tail up to 28 bytes each.
     * As in the initial byte-based head/tail fill, each
     * conditional below ensures that the subsequent offsets
     * are valid (e.g. !(n<=24) implies n>=28). */

    *(u32 *)(s+0) = c32;
    *(u32 *)(s+n-4) = c32;
    if (n <= 8) return dest;
    *(u32 *)(s+4) = c32;
    *(u32 *)(s+8) = c32;
    *(u32 *)(s+n-12) = c32;
    *(u32 *)(s+n-8) = c32;
    if (n <= 24) return dest;
    *(u32 *)(s+12) = c32;
    *(u32 *)(s+16) = c32;
    *(u32 *)(s+20) = c32;
    *(u32 *)(s+24) = c32;
    *(u32 *)(s+n-28) = c32;
    *(u32 *)(s+n-24) = c32;
    *(u32 *)(s+n-20) = c32;
    *(u32 *)(s+n-16) = c32;

    /* Align to a multiple of 8 so we can fill 64 bits at a time,
     * and avoid writing the same bytes twice as much as is
     * practical without introducing additional branching. */

    k = 24 + ((uintptr_t)s & 4);
    s += k;
    n -= k;

    /* If this loop is reached, 28 tail bytes have already been
     * filled, so any remainder when n drops below 32 can be
     * safely ignored. */

    u64 c64 = c32 | ((u64)c32 << 32);
    for (; n >= 32; n-=32, s+=32) {
        *(u64 *)(s+0) = c64;
        *(u64 *)(s+8) = c64;
        *(u64 *)(s+16) = c64;
        *(u64 *)(s+24) = c64;
    }
#else
    /* Pure C fallback with no aliasing violations. */
    for (; n; n--, s++) *s = c;
#endif

    return dest;
}

int memcmp(const void *vl, const void *vr, size_t n)
{
    const unsigned char *l=vl, *r=vr;
    for (; n && *l == *r; n--, l++, r++);
    return n ? *l-*r : 0;
}

size_t strlen(const char *s) {
    const char *start = s;
    for (; *s; s++);
    return s - start;
}

char *strcpy(char *restrict dest, const char *src) {
    for (; (*dest=*src); src++, dest++);
    return dest;
}

int strncmp(const char *_l, const char *_r, size_t n)
{
    const unsigned char *l=(void *)_l, *r=(void *)_r;
    if (!n--) return 0;
    for (; *l && *r && n && *l == *r ; l++, r++, n--);
    return *l - *r;
}

char *strcat(char *restrict dest, const char *restrict src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

