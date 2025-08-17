#include <stdio.h>

int main()
{
    printf("\x1b[47;90mHello, World!\x1b[49m\n"); 
    printf("\x1b[91mHello, World!\n"); //红色
    printf("\x1b[92mHello, World!\n"); //绿色
    printf("\x1b[93mHello, World!\n"); //黄色
    printf("\x1b[94mHello, World!\n"); //蓝色
    printf("\x1b[95mHello, World!\n"); //粉色
    printf("\x1b[96mHello, World!\n"); //青色
    printf("\x1b[97mHello, World!\n"); //白色
    return 0;
}