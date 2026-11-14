/* Compiler lowering support for the opt-in firmware probe only.
 * Volatile byte accesses prevent loop-to-libcall recursion under GCC/Clang. */
#include <stddef.h>
void *memset(void *dst, int value, size_t count)
{
    volatile unsigned char *p = dst;
    while (count--) *p++ = (unsigned char)value;
    return dst;
}
void *memcpy(void *dst, const void *src, size_t count)
{
    volatile unsigned char *p = dst;
    const volatile unsigned char *q = src;
    while (count--) *p++ = *q++;
    return dst;
}
