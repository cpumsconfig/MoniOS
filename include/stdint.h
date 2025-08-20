#ifndef _STDINT_H_
#define _STDINT_H_

#if defined(__x86_64__) || defined(_M_X64)
typedef unsigned long size_t;
#elif defined(__i386__) || defined(_M_IX86)
typedef unsigned int size_t;
#else
// 默认回退：假设 32 位
typedef unsigned int size_t;
#endif
#if __WORDSIZE == 64
typedef unsigned long int uintptr_t;
#else
typedef unsigned int uintptr_t;
#endif
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef unsigned short uint16_t;
typedef          short int16_t;
typedef unsigned char  uint8_t;
typedef          char  int8_t;

#endif