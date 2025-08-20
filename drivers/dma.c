#include "drivers/dma.h"
#include "monios/common.h"

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

// 设置DMA传输（内部使用）
void dma_setup(uint8_t channel, uint32_t addr, uint16_t count, uint8_t mode) {
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
    outb(count_port, (count - 1) & 0xFF);
    outb(count_port, ((count - 1) >> 8) & 0xFF);

    // 启用指定通道
    outb(DMA0_MASK_REG, channel);
}

// 提供给外部的统一接口：启动DMA传输
int dma_start_transfer(uint8_t channel, size_t size, uint8_t mode) {
    if (size == 0) {
        return -1;
    }

    // 转换为字节数（8位通道用字节计数）
    if (size > 0xFFFF) {
        size = 0xFFFF; // DMA 单次最大 64KB
    }

    // 这里假设物理地址和虚拟地址相同
    uint32_t phys_addr = (uint32_t)0x100000; // ⚠️ 需要替换成真正的缓冲区地址

    dma_setup(channel, phys_addr, (uint16_t)size, mode);
    return 0;
}

// 从I/O设备到内存的DMA传输
void dma_read(uint8_t channel, uint32_t addr, uint16_t count) {
    dma_setup(channel, addr, count, DMA_MODE_READ);
}

// 从内存到I/O设备的DMA传输
void dma_write(uint8_t channel, uint32_t addr, uint16_t count) {
    dma_setup(channel, addr, count, DMA_MODE_WRITE);
}

// 初始化指定的DMA通道
void dma_init_channel(uint8_t channel) {
    if (channel > 3) return;  // 只支持0-3通道
    outb(DMA0_MASK_REG, channel | 0x04);  // 禁用通道
    outb(DMA0_CLEAR_FF_REG, 0);  // 清除flip-flop
    outb(DMA0_MODE_REG, DMA_MODE_SINGLE | channel);  // 设置单次传输模式
}

// 获取DMA缓冲区地址
void* dma_get_buffer(uint8_t channel) {
    // 这里返回一个固定的DMA缓冲区地址
    // 实际应用中应该根据channel返回对应的缓冲区地址
    return (void*)0x100000;  // 1MB处的缓冲区
}

