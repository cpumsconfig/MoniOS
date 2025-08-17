#include "net.h"
#include "stdbool.h"

// 网络初始化包装函数
void init_network_c(void) {
    init_network();
}

// 发送数据包包装函数
void send_packet_c(uint8_t *data, uint16_t len) {
    send_packet(data, len);
}

// Ping实现包装函数
void do_ping_impl_c(const char *ip_str) {
    do_ping_impl(ip_str);
}

// 网络中断处理包装函数
void net_interrupt_handler_c(struct regs *r) {
    // 检查是否是网络中断
    uint8_t irq = r->int_no - 32;
    if (irq == 10) {  // NIC_IRQ
        net_interrupt_handler();
    }
}

// 端口I/O函数
static inline uint8_t inb1(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb1(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// 实现print_str函数，供network.asm使用
void print_str(const char* str) {
    monitor_printf("%s", str);
}

// 实现put_char函数，供network.asm使用
void put_char(char c) {
    monitor_put(c);
}
