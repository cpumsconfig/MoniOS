#ifndef __SB16_H
#define __SB16_H

#include <stdint.h>
#include "common.h"
#include "dma.h"
#include "isr.h"

// SB16 默认基地址
#define SB16_BASE 0x220

// DSP 端口
#define DSP_RESET        (SB16_BASE + 0x6)
#define DSP_READ         (SB16_BASE + 0xA)
#define DSP_WRITE_CMD    (SB16_BASE + 0xC)
#define DSP_READ_STATUS  (SB16_BASE + 0xE)

// 混音器端口
#define MIXER_ADDR       (SB16_BASE + 0x4)
#define MIXER_DATA       (SB16_BASE + 0x5)

// DMA 通道 (SB16 默认使用 DMA 通道 5)
#define SB16_DMA_CHANNEL 5

// 函数声明
void sb16_init(void);
void sb16_play(uint8_t* data, uint32_t size, uint16_t sample_rate);
void sb16_stop(void);
void sb16_irq_handler(registers_t* regs);

#endif // __SB16_H