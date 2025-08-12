#include "time.h"
#include "stdio.h" // 为了 printf (如果需要在time.c内部调试)

// 定义寄存器结构体，用于内联汇编调用BIOS中断
struct regs {
    unsigned short ax, bx, cx, dx, si, di, ds, es;
};

// 内部函数：调用BIOS中断
extern void bios_int(struct regs *r, int interrupt);



// 内部函数：从BCD码转换为整数
static int bcd_to_int(int bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 内部函数：获取当前时间的各个组成部分
static void get_time_components(int *century, int *year, int *month, int *day, int *weekday, int *hour, int *minute, int *second) {
    struct regs r = {0};

    // 1. 获取日期 (INT 1Ah, AH=04h)
    r.ax = 0x0400;
    bios_int(0x1A, &r);
    // CX = 世纪:年 (BCD), DX = 月:日 (BCD)
    int century_bcd = (r.cx >> 8) & 0xFF;
    int year_bcd = r.cx & 0xFF;
    int month_bcd = (r.dx >> 8) & 0xFF;
    int day_bcd = r.dx & 0xFF;
    
    *century = bcd_to_int(century_bcd);
    *year = bcd_to_int(year_bcd);
    *month = bcd_to_int(month_bcd);
    *day = bcd_to_int(day_bcd);

    // 2. 获取时间和星期 (INT 1Ah, AH=02h)
    r.ax = 0x0200;
    bios_int(0x1A, &r);
    // CH = 小时 (BCD), CL = 分钟 (BCD), DH = 秒 (BCD), DL = 星期 (0=Sunday)
    int hour_bcd = (r.cx >> 8) & 0xFF;
    int minute_bcd = r.cx & 0xFF;
    int second_bcd = (r.dx >> 8) & 0xFF;
    *weekday = r.dx & 0xFF; // 星期是直接返回的，不是BCD

    *hour = bcd_to_int(hour_bcd);
    *minute = bcd_to_int(minute_bcd);
    *second = bcd_to_int(second_bcd);
}

// 内部函数：一个简单的忙等待延时
// 注意：这个延时非常不精确，取决于CPU速度。
// 在一个真正的OS中，你会使用硬件定时器。
static void delay_ms(unsigned int ms) {
    // 这是一个非常粗略的估计。需要根据你的CPU速度进行校准。
    // 假设一个简单的循环大约需要1微秒 (1/1000毫秒)
    // 这个值需要根据你的模拟器或硬件进行调整。
    volatile unsigned int loops_per_ms = 1000; 
    for (unsigned int i = 0; i < ms; ++i) {
        for (volatile unsigned int j = 0; j < loops_per_ms; ++j) {
            // 空循环，消耗CPU周期
        }
    }
}


// --- 公共接口实现 ---

char* data(const char* format) {
    static char buffer[256]; // 静态缓冲区，用于存储结果
    int century, year, month, day, weekday, hour, minute, second;
    
    get_time_components(&century, &year, &month, &day, &weekday, &hour, &minute, &second);

    int buffer_index = 0;
    for (int i = 0; format[i] != '\0'; ++i) {
        if (format[i] == '\\') { // 处理转义字符
            i++;
            if (format[i] == '\0') break;
            buffer[buffer_index++] = format[i];
            continue;
        }

        if (format[i] != '%') {
            buffer[buffer_index++] = format[i];
            continue;
        }

        // 处理格式化占位符
        i++; // 移动到 '%' 后面的字符
        char specifier = format[i];
        int temp_num;
        char temp_str[5]; // 足够存放4位年份和'\0'

        switch (specifier) {
            case 'Y': // 四位年份
                sprintf(temp_str, "%04d", century * 100 + year);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 'y': // 两位年份
                sprintf(temp_str, "%02d", year);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 'm': // 月
                sprintf(temp_str, "%02d", month);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 'd': // 日
                sprintf(temp_str, "%02d", day);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 'l': // 星期
                sprintf(temp_str, "%d", weekday);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 'H': // 24小时制
                sprintf(temp_str, "%02d", hour);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 'h': { // 12小时制
                temp_num = hour % 12;
                if (temp_num == 0) temp_num = 12;
                sprintf(temp_str, "%02d", temp_num);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            }
            case 'i': // 分钟
                sprintf(temp_str, "%02d", minute);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            case 's': // 秒
                sprintf(temp_str, "%02d", second);
                for(int j=0; temp_str[j]!='\0'; j++) buffer[buffer_index++] = temp_str[j];
                break;
            default: // 未知的占位符，原样输出
                buffer[buffer_index++] = '%';
                buffer[buffer_index++] = specifier;
                break;
        }
    }
    buffer[buffer_index] = '\0'; // 字符串终止符

    return buffer;
}

char* getdata() {
    // 直接调用 data 函数，使用预定义的格式
    return data("YmdHis");
}

void sleep(unsigned int milliseconds) {
    delay_ms(milliseconds);
}
