#ifndef __DMA_H
#define __DMA_H

#include <stdint.h>
#include "common.h"  // 包含outb和inb函数的声明

// DMA控制器端口地址
#define DMA0_CMD_REG     0x08
#define DMA0_STAT_REG    0x08
#define DMA0_REQ_REG     0x09
#define DMA0_MASK_REG    0x0A
#define DMA0_MODE_REG    0x0B
#define DMA0_CLEAR_FF_REG 0x0C
#define DMA0_TEMP_REG    0x0D
#define DMA0_MASTER_CLEAR 0x0D

// DMA通道寄存器
#define DMA0_CHAN0_ADDR_REG 0x00
#define DMA0_CHAN0_COUNT_REG 0x01
#define DMA0_CHAN1_ADDR_REG 0x02
#define DMA0_CHAN1_COUNT_REG 0x03
#define DMA0_CHAN2_ADDR_REG 0x04
#define DMA0_CHAN2_COUNT_REG 0x05
#define DMA0_CHAN3_ADDR_REG 0x06
#define DMA0_CHAN3_COUNT_REG 0x07

// DMA传输模式
#define DMA_MODE_READ    0x44  // 从I/O到内存
#define DMA_MODE_WRITE   0x48  // 从内存到I/O
#define DMA_MODE_AUTO    0x10  // 自动初始化

// DMA通道掩码
#define DMA_CHAN0_ENABLE 0x00
#define DMA_CHAN0_DISABLE 0x04
#define DMA_CHAN1_ENABLE  0x01
#define DMA_CHAN1_DISABLE 0x05
#define DMA_CHAN2_ENABLE  0x02
#define DMA_CHAN2_DISABLE 0x06
#define DMA_CHAN3_ENABLE  0x03
#define DMA_CHAN3_DISABLE 0x07
#define DMA_MODE_SINGLE 0x40

// 函数声明
void dma_init(void);
void dma_setup(uint8_t channel, uint32_t addr, uint16_t count, uint8_t mode);
void dma_read(uint8_t channel, uint32_t addr, uint16_t count);
void dma_write(uint8_t channel, uint32_t addr, uint16_t count);

#endif // __DMA_H
