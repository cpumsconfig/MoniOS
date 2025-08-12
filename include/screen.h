// screen.h
#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

// VGA屏幕信息结构体
#pragma pack(push, 1)
typedef struct {
    uint8_t  mode;
    uint16_t width;
    uint16_t height;
    uint32_t vram_addr;
} VGA_Info;
#pragma pack(pop)

// 设置VGA模式为320x200 256色
void set_vga_mode();

// 清屏函数(全黑)
void clear_screen();

// 获取当前VGA信息
const VGA_Info* get_vga_info();

// 在指定位置绘制一个像素
void draw_pixel(uint16_t x, uint16_t y, uint8_t color);

#endif