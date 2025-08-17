#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>

// 寄存器结构定义
struct regs {
    uint32_t ds, es, fs, gs;      // 数据段选择子
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // 通用寄存器
    uint32_t int_no, err_code;   // 中断号和错误码
    uint32_t eip, cs, eflags, useresp, ss; // EIP, CS, EFLAGS, ESP和SS
};

// 从network.asm导入的函数声明
extern void init_network(void);
extern void send_packet(uint8_t *data, uint16_t len);
extern void do_ping_impl(const char *ip_str);
extern void net_interrupt_handler(void);

// C语言包装函数声明
void init_network_c(void);
void send_packet_c(uint8_t *data, uint16_t len);
void do_ping_impl_c(const char *ip_str);
void net_interrupt_handler_c(struct regs *r);

// 网络初始化和中断控制函数
void late_init_network(void);
void enable_network_interrupts(void);
void init_pic(void);

// 辅助函数声明
void print_str(const char* str);
void put_char(char c);

#endif // NET_H
