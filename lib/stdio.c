#include "monios/common.h"
#include "stdarg.h" // 在开头添加，因为用到了va_list以及操纵va_list的这些东西

typedef unsigned int size_t;

// 字符串连接函数
char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
    return ret;
}

// 查找字符串中最后一个出现的指定字符
char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == c) {
            last = s;
        }
        s++;
    }
    return (char *)last;
}


int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = 0;
    char *ptr = buf;
    
    while (*fmt && count < size - 1) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd': {
                    int num = va_arg(args, int);
                    char num_buf[32];
                    int i = 0;
                    
                    if (num < 0) {
                        *ptr++ = '-';
                        count++;
                        num = -num;
                    }
                    
                    do {
                        num_buf[i++] = '0' + (num % 10);
                        num /= 10;
                    } while (num > 0);
                    
                    while (--i >= 0 && count < size - 1) {
                        *ptr++ = num_buf[i];
                        count++;
                    }
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    while (*str && count < size - 1) {
                        *ptr++ = *str++;
                        count++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    if (count < size - 1) {
                        *ptr++ = c;
                        count++;
                    }
                    break;
                }
                default:
                    if (count < size - 1) {
                        *ptr++ = *fmt;
                        count++;
                    }
            }
        } else {
            *ptr++ = *fmt;
            count++;
        }
        fmt++;
    }
    
    *ptr = '\0';
    va_end(args);
    return count;
}

static void itoa(uint32_t num, char **buf_ptr_addr, int radix)
{
    uint32_t m = num % radix; // 最低位
    uint32_t i = num / radix; // 最高位
    if (i) itoa(i, buf_ptr_addr, radix); // 先把高位化为字符串
    if (m < 10) { // 处理最低位
        *((*buf_ptr_addr)++) = m + '0'; // 0~9，直接加0
    } else {
        *((*buf_ptr_addr)++) = m - 10 + 'A'; // 10~15，10~15->0~5->A~F
    }
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    char *buf_ptr = buf; // 不动原来的buf，原来的buf可能还用得着
    const char *index_ptr = fmt; // 不动原来的fmt，但这个好像真用不着
    char index_char = *index_ptr; // fmt串中的当前字符
    int32_t arg_int; // 可能会出现的int参数
    char *arg_str; // 可能会出现的char *参数
    while (index_char) { // 没到fmt的结尾
        if (index_char != '%') { // 不是%
            *(buf_ptr++) = index_char; // 直接复制到buf
            index_char = *(++index_ptr); // 自动更新到下一个字符
            continue; // 跳过后续对于%的判断
        }
        index_char = *(++index_ptr); // 先把%跳过去
        switch (index_char) { // 对现在的index_char进行判断
            case 's':
                arg_str = va_arg(ap, char*); // 获取char *参数
                strcpy(buf_ptr, arg_str); // 直接strcpy进buf_ptr
                buf_ptr += strlen(arg_str); // buf_ptr直接跳到arg_str结尾，正好在arg_str结尾的\0处
                break;
            case 'c':
                *(buf_ptr++) = va_arg(ap, int); // 把获取到的char参数直接写进buf_ptr
                break;
            case 'x':
                arg_int = va_arg(ap, int); // 获取int参数
                itoa(arg_int, &buf_ptr, 16); // itoa早在设计时就可以修改buf_ptr，这样就直接写到buf_ptr里了，还自动跳到数末尾
                break;
            case 'd':
                arg_int = va_arg(ap, int); // 获取int参数
                if (arg_int < 0) { // 给负数前面加个符号
                    arg_int = -arg_int; // 先转负为正
                    *(buf_ptr++) = '-'; // 然后加负号
                }
                itoa(arg_int, &buf_ptr, 10); // itoa早在设计时就可以修改buf_ptr，这样就直接写到buf_ptr里了，还自动跳到数末尾
                break;
            default:
                break;
        }
        index_char = *(++index_ptr); // 再把%后面的s c x d跳过去
    }
    return strlen(buf); // 返回做完后buf的长度
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsprintf(buf, fmt, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *fmt, va_list ap)
{
    char buf[1024] = {0}; // 理论上够了
    int ret = vsprintf(buf, fmt, ap);
    write(1, buf, ret);
    return ret;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void puts(const char *buf)
{
    write(1, buf, strlen(buf));
    write(1, "\n", 1);
}

int putchar(char ch)
{
    printf("%c", ch);
    return ch;
}