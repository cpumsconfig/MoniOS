#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// 屏幕尺寸常量
#define VGA_WIDTH  320
#define VGA_HEIGHT 200
#define VGA_SIZE   (VGA_WIDTH * VGA_HEIGHT)

// 显存起始地址
#define VGA_MEMORY ((uint8_t*)0xA0000)

// 颜色定义 (RGB332格式)
typedef enum {
    VGA_BLACK     = 0x00,
    VGA_BLUE      = 0x03,
    VGA_GREEN     = 0x1C,
    VGA_CYAN      = 0x1F,
    VGA_RED       = 0xC0,
    VGA_MAGENTA   = 0xC3,
    VGA_BROWN     = 0xD4,
    VGA_LIGHT_GRAY= 0xD6,
    VGA_DARK_GRAY = 0x92,
    VGA_LIGHT_BLUE= 0x97,
    VGA_LIGHT_GREEN=0x9E,
    VGA_LIGHT_CYAN= 0xBF,
    VGA_LIGHT_RED = 0xF0,
    VGA_PINK      = 0xF3,
    VGA_YELLOW    = 0xFC,
    VGA_WHITE     = 0xFF
} VGA_Color;

// 函数声明
void vga_set_mode_13h();
void vga_clear_screen(uint8_t color);
void vga_draw_pixel(int x, int y, uint8_t color);
void vga_draw_rect(int x, int y, int width, int height, uint8_t color);
void vga_draw_gradient();

#endif // VGA_H