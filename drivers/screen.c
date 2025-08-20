#include "drivers/screen.h"
#include "monios/common.h"

// VGA寄存器端口定义
#define SEQ_ADDR   0x3C4
#define SEQ_DATA   0x3C5
#define CRT_ADDR   0x3D4
#define CRT_DATA   0x3D5
#define MISC_ADDR  0x3CC
#define MISC_WRITE 0x3C2
#define GC_ADDR    0x3CE
#define GC_DATA    0x3CF

//切换文本模式为图形vga

void vga_set_mode_13h() {
    // 禁用中断
    asm volatile("cli");

    // 设置Miscellaneous寄存器
    uint8_t misc = inb(MISC_ADDR);
    outb(MISC_WRITE, misc | 0x01);  // 选择彩色模式

    // 解锁CRTC寄存器（禁用写保护）
    outb(CRT_ADDR, 0x11);           // 选择垂直结束寄存器
    uint8_t cr11 = inb(CRT_DATA);
    outb(CRT_DATA, cr11 & 0x7F);    // 清除最高位

    // 设置Sequencer寄存器
    const uint8_t seq_regs[] = {0x03, 0x01, 0x0F, 0x00, 0x06};
    for (uint8_t i = 0; i < 5; i++) {
        outb(SEQ_ADDR, i);
        outb(SEQ_DATA, seq_regs[i]);
    }

    // 设置CRT控制器寄存器
    const uint8_t crt_regs[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x8E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
        0xFF
    };
    for (uint8_t i = 0; i < 25; i++) {
        outb(CRT_ADDR, i);
        outb(CRT_DATA, crt_regs[i]);
    }

    // 恢复CRTC写保护
    outb(CRT_ADDR, 0x11);
    outb(CRT_DATA, cr11);

    // 设置Graphics Controller
    const uint8_t gfx_regs[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F};
    for (uint8_t i = 0; i < 8; i++) {
        outb(GC_ADDR, i);
        outb(GC_DATA, gfx_regs[i]);
    }

    // 启用中断
    asm volatile("sti");
}


void vga_clear_screen(uint8_t color) {
    uint8_t *vga = VGA_MEMORY;
    for (int i = 0; i < VGA_SIZE; i++) {
        vga[i] = color;
    }
}

void vga_draw_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        VGA_MEMORY[y * VGA_WIDTH + x] = color;
    }
}

void vga_draw_rect(int x, int y, int width, int height, uint8_t color) {
    // 裁剪到屏幕边界
    int x_end = (x + width > VGA_WIDTH) ? VGA_WIDTH : x + width;
    int y_end = (y + height > VGA_HEIGHT) ? VGA_HEIGHT : y + height;
    
    for (int cy = (y < 0) ? 0 : y; cy < y_end; cy++) {
        for (int cx = (x < 0) ? 0 : x; cx < x_end; cx++) {
            vga_draw_pixel(cx, cy, color);
        }
    }
}

// 示例：绘制彩色渐变
void vga_draw_gradient() {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            // 创建渐变效果 (RGB332)
            uint8_t r = (x * 8) / VGA_WIDTH;   // 红色分量 (3位)
            uint8_t g = (y * 8) / VGA_HEIGHT;  // 绿色分量 (3位)
            uint8_t b = (x + y) * 4 / (VGA_WIDTH + VGA_HEIGHT); // 蓝色分量 (2位)
            
            uint8_t color = (r << 5) | (g << 2) | b;
            vga_draw_pixel(x, y, color);
        }
    }
}