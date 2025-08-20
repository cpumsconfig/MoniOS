#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include <stdbool.h>

// 计时器基准频率（PIT 固定）
#define PIT_BASE_HZ 1193182

// 初始化 PIT，设置期望频率（Hz）。常用 100 或 1000。
bool pit_init(uint32_t hz);

// 当前 PIT 频率（成功 init 后保存）
uint32_t pit_get_frequency(void);

// 从启动后累计的 tick 数（自增于 IRQ0）
uint64_t pit_get_ticks(void);

// 毫秒级等待；要求已 pit_init() 且已开启中断（sti）
void timer_wait_ms(uint32_t ms);

// 可选：微秒级的粗略忙等（无中断需求，误差较大）
void timer_udelay(uint32_t usec);

#endif // PIT_H
