#ifndef _STDIO_H_
#define _STDIO_H_

#include "string.h"
#include "stdint.h"
#include "stdarg.h"

typedef unsigned int size_t;

int vsprintf(char *buf, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int printf(const char *fmt, ...);
void puts(const char *buf);
int putchar(char ch);
int snprintf(char *buf, size_t size, const char *fmt, ...);

int printk(const char *fmt, ...); // for kernel use

#endif