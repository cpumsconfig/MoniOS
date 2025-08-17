#include "common.h"
#include "stduint.h"

void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %1, %0" : : "dN"(port), "a"(value)); // 相当于 out value, port
}

void outw(uint16_t port, uint16_t value)
{
    asm volatile("outw %1, %0" : : "dN"(port), "a"(value)); // 相当于 out value, port
}

uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port)); // 相当于 in val, port; return val;
    return ret;
}

uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port)); // 相当于 in val, port; return val;
    return ret;
}

u32 inl(u16 port) {
   u32 ret;
   asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

void outl(u16 port, u32 value) {
   asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}
