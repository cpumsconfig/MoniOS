#ifndef COMMON_H
#define COMMON_H

#include "stduint.h"

typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef unsigned short uint16_t;
typedef          short int16_t;
typedef unsigned char  uint8_t;
typedef          char  int8_t;

/* typedef int8_t bool;
#define true 1
#define false 0 */

void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void outl(u16 port, u32 value);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
u32 inl(u16 port);


#define NULL ((void *) 0)

#include "string.h"

#endif