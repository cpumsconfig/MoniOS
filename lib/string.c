#include "monios/common.h"

void *memset(void *dst_, uint8_t value, uint32_t size)
{
    uint8_t *dst = (uint8_t *) dst_;
    while (size-- > 0) *dst++ = value;
    return dst_;
}

void *memcpy(void *dst_, const void *src_, uint32_t size)
{
    uint8_t *dst = dst_;
    const uint8_t *src = src_;
    while (size-- > 0) *dst++ = *src++;
    return (void *) src_;
}

int memcmp(const void *a_, const void *b_, uint32_t size)
{
    const char *a = a_;
    const char *b = b_;
    while (size-- > 0) {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

char *strcpy(char *dst_, const char *src_)
{
    char *r = dst_;
    while ((*dst_++ = *src_++));
    return r;
}

char *strncpy(char *dst_, const char *src_, uint32_t n)  // 添加这个函数
{
    char *r = dst_;
    while (n-- > 0) {
        if ((*dst_++ = *src_++) == '\0') {
            // 如果源字符串结束，用'\0'填充剩余空间
            while (n-- > 0) *dst_++ = '\0';
            break;
        }
    }
    return r;
}

uint32_t strlen(const char *str)
{
    const char *p = str;
    while (*p++);
    return p - str - 1;
}

int8_t strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) a++, b++;
    return *a < *b ? -1 : *a > *b;
}

char *strchr(const char *str, const uint8_t ch)
{
    while (*str) {
        if (*str == ch) return (char *) str;
        str++;
    }
    return NULL;
}