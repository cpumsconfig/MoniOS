#ifndef TIME_H
#define TIME_H

#include <stddef.h> // 为了 size_t 类型，如果需要的话

// 函数声明

/**
 * @brief 根据格式字符串获取并格式化当前时间。
 * 
 * @param format 一个格式字符串，包含以下占位符：
 *               Y: 四位年份 (e.g., 2025)
 *               y: 两位年份 (e.g., 25)
 *               m: 月份 (01-12)
 *               d: 日期 (01-31)
 *               l: 星期 (0-6, where 0 is Sunday)
 *               H: 24小时制小时 (00-23)
 *               h: 12小时制小时 (01-12)
 *               i: 分钟 (00-59)
 *               s: 秒 (00-59)
 * @return char* 指向一个静态字符串缓冲区，包含格式化后的时间。
 *               注意：每次调用都会覆盖这个缓冲区。
 */
char* data(const char* format);

/**
 * @brief 获取一个紧凑的、无分隔符的时间字符串。
 * 
 * 格式为 "YYYYMMDDHHMMSS"。
 * 
 * @return char* 指向一个静态字符串缓冲区，包含紧凑格式的时间。
 *               注意：每次调用都会覆盖这个缓冲区。
 */
char* getdata();

/**
 * @brief 暂停执行指定的毫秒数。
 * 
 * @param milliseconds 要暂停的毫秒数。
 */
void sleep(unsigned int milliseconds);

#endif // TIME_H
