#include "dma.h"

// 初始化DMA控制器
void dma_init(void) {
    // 禁用所有DMA通道
    outb(DMA0_MASK_REG, DMA_CHAN0_DISABLE);
    outb(DMA0_MASK_REG, DMA_CHAN1_DISABLE);
    outb(DMA0_MASK_REG, DMA_CHAN2_DISABLE);
    outb(DMA0_MASK_REG, DMA_CHAN3_DISABLE);
    
    // 主清除
    outb(DMA0_MASTER_CLEAR, 0);
}

// 设置DMA传输
void dma_setup(uint8_t channel, uint32_t addr, uint16_t count, uint8_t mode) {
    // 计算实际传输的字节数（count是字数，乘以2得到字节数）
    uint16_t bytes = count * 2;
    
    // 确保地址不超过24位（DMA控制器限制）
    if (addr > 0xFFFFFF) {
        return; // 地址超出范围
    }
    
    // 禁用指定通道
    outb(DMA0_MASK_REG, channel | 0x04);
    
    // 清除 flip-flop 寄存器
    outb(DMA0_CLEAR_FF_REG, 0);
    
    // 设置传输模式
    outb(DMA0_MODE_REG, mode | channel);
    
    // 设置地址寄存器
    uint8_t addr_port = DMA0_CHAN0_ADDR_REG + (channel * 2);
    outb(addr_port, addr & 0xFF);
    outb(addr_port, (addr >> 8) & 0xFF);
    
    // 设置页面寄存器（地址的高8位）
    uint8_t page_port = 0x87 + channel; // 页面寄存器端口
    outb(page_port, (addr >> 16) & 0xFF);
    
    // 清除 flip-flop 寄存器
    outb(DMA0_CLEAR_FF_REG, 0);
    
    // 设置计数寄存器（传输的字节数减1）
    uint8_t count_port = DMA0_CHAN0_COUNT_REG + (channel * 2);
    outb(count_port, (bytes - 1) & 0xFF);
    outb(count_port, ((bytes - 1) >> 8) & 0xFF);
    
    // 启用指定通道
    outb(DMA0_MASK_REG, channel);
}

// 从I/O设备到内存的DMA传输
void dma_read(uint8_t channel, uint32_t addr, uint16_t count) {
    dma_setup(channel, addr, count, DMA_MODE_READ);
}

// 从内存到I/O设备的DMA传输
void dma_write(uint8_t channel, uint32_t addr, uint16_t count) {
    dma_setup(channel, addr, count, DMA_MODE_WRITE);
}
