#include "screen.h"

int main(){
    vga_set_mode_13h();      // 切换到图形模式
    vga_clear_screen(VGA_BLUE);
}