#include "drivers/pit.h"
#include <stdint.h>
#include <stdbool.h>

// 你工程里的端口/中断接口（按你的工程头文件来调整）
#include "monios/common.h"     // outb/inb 原型（如果你在别的头里，就改成对应的头）
#include "drivers/isr.h"        // register_interrupt_handler, 中断号定义等

// PIT 端口
#define PIT_CH0_DATA  0x40
#define PIT_MODE_CMD  0x43

// PIT 模式字：通道0、访问低字节后高字节、方式2（Rate Generator）、二进制计数
#define PIT_MODE_CH0_LH_MODE2  0x34  // 00 11 010 0  -> 0x34

static volatile uint64_t s_pit_ticks = 0;
static uint32_t s_pit_hz = 0;
static bool s_pit_inited = false;

// IRQ0 处理函数（PIT）
static void pit_irq_handler(registers_t *regs) {
    (void)regs;
    s_pit_ticks++;
    // 通常在通用 IRQ 框架里已自动发送 EOI，这里无需重复
}

bool pit_init(uint32_t hz) {
    if (hz == 0) return false;

    // 计算分频值（1~65535）
    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 65535) divisor = 65535;

    // 设定模式字
    outb(PIT_MODE_CMD, PIT_MODE_CH0_LH_MODE2);

    // 写入分频值（低字节、再高字节）
    outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    s_pit_hz = PIT_BASE_HZ / divisor; // 实际频率（四舍五入偏差）
    s_pit_ticks = 0;

    // 挂载 IRQ0 处理函数（你工程里 IRQ0 编号一般是 32 或者宏 IRQ0）
    register_interrupt_handler(IRQ0, pit_irq_handler);

    s_pit_inited = true;
    return true;
}

uint32_t pit_get_frequency(void) {
    return s_pit_hz;
}

uint64_t pit_get_ticks(void) {
    return s_pit_ticks;
}

void timer_wait_ms(uint32_t ms) {
    if (!s_pit_inited || s_pit_hz == 0) {
        volatile uint32_t spin = ms * 100000U;
        while (spin--) { __asm__ volatile("nop"); }
        return;
    }

    uint32_t start = s_pit_ticks;
    uint32_t target = start + (ms * s_pit_hz) / 1000U;
    if (target == start) target = start + 1;

    while (s_pit_ticks < target) {
        __asm__ volatile ("hlt");
    }
}

void timer_udelay(uint32_t usec) {
    // 简易忙等（不依赖中断），粗糙但可用
    // 常量需按你机器频率微调以得到合适的延时
    volatile uint32_t loops = usec * 25; // 经验值，必要时自行微调
    while (loops--) { __asm__ volatile("nop"); }
}
