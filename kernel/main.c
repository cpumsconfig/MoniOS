#include "monitor.h"
#include "gdtidt.h"
#include "isr.h"
#include "timer.h"
#include "memory.h"
#include "mtask.h"
#include "keyboard.h"
#include "shell.h"
#include "cmos.h"
#include "file.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "shutdown.h"
#include "net.h"
#include "execute.h"
#include "dma.h"
#include "sb16.h"

// 定义缺失的段选择子常量
#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define DRIVER_CODE_SELECTOR 0x1B
#define DRIVER_DATA_SELECTOR 0x23
#define USER_CODE_SELECTOR   0x2B
#define USER_DATA_SELECTOR   0x33

#define MAX_CMD_LEN 128
#define MAX_ARG_NUM 32
#define KERNEL_STACK_SIZE (64 * 1024)

// Shell 全局状态
typedef struct {
    char input_buffer[MAX_CMD_LEN];
    char backup_buffer[MAX_CMD_LEN];
    char *arguments[MAX_ARG_NUM];
    int argc;
} shell_state_t;

static shell_state_t shell_state;
// 函数声明
int fat16_open_dir(fileinfo_t *finfo, const char *path);
int fat16_read_dir(fileinfo_t *finfo, char *filename);
// 在其他包含头文件后添加
extern void set_vga_mode(void);
extern void call_bios_int(void);
// 任务创建函数
task_t *create_kernel_task(void *entry, int privilege_level)
{
    task_t *new_task = task_alloc();
    if (!new_task) {
        monitor_printf("Failed to allocate task structure\n");
        return NULL;
    }
    
    // 分配内核栈
    void *stack_base = kmalloc(KERNEL_STACK_SIZE);
    if (!stack_base) {
        task_free(new_task);
        monitor_printf("Failed to allocate task stack\n");
        return NULL;
    }
    
    new_task->tss.esp = (uint32_t)stack_base + KERNEL_STACK_SIZE - sizeof(uint32_t);
    new_task->tss.eip = (uint32_t)entry;

    // 根据特权级设置段选择子
    uint16_t code_sel, data_sel;
    
    switch (privilege_level) {
        case 0: // 内核级 (Ring 0)
            code_sel = KERNEL_CODE_SELECTOR;
            data_sel = KERNEL_DATA_SELECTOR;
            break;
        case 1: // 驱动级 (Ring 1)
            code_sel = DRIVER_CODE_SELECTOR;
            data_sel = DRIVER_DATA_SELECTOR;
            break;
        case 3: // 用户级 (Ring 3)
            code_sel = USER_CODE_SELECTOR;
            data_sel = USER_DATA_SELECTOR;
            break;
        default: // 默认为内核级
            code_sel = KERNEL_CODE_SELECTOR;
            data_sel = KERNEL_DATA_SELECTOR;
            privilege_level = 0;
            break;
    }
    
    monitor_printf("Creating task at 0x%x with privilege level %d\n", 
                   (uint32_t)entry, privilege_level);

    // 设置任务状态段
    new_task->tss.cs = code_sel;
    new_task->tss.ds = data_sel;
    new_task->tss.es = data_sel;
    new_task->tss.ss = data_sel;
    new_task->tss.fs = data_sel;
    new_task->tss.gs = data_sel;
    new_task->tss.eflags = 0x202; // 中断使能
    
    return new_task;
}

// ==================== Shell 功能实现 ====================

static void display_prompt() 
{
    printf("[monios@user /] $ ");
}

// 解决 size_t 问题
static void read_command_line(char *buffer, unsigned int max_len)
{
    char *ptr = buffer;
    unsigned int count = 0;
    
    while (count < max_len - 1) {
        char c;
        if (read(0, &c, 1) <= 0) continue;
        
        switch (c) {
            case '\n':
            case '\r': // 回车结束输入
                *ptr = '\0';
                putchar('\n');
                return;
                
            case '\b': // 处理退格
                if (ptr > buffer) {
                    ptr--;
                    count--;
                    putchar('\b');
                    putchar(' ');
                    putchar('\b');
                }
                break;
                
            case 0x03: // Ctrl+C
                *buffer = '\0';
                printf("^C\n");
                return;
                
            default: // 普通字符
                if (c >= 32 && c <= 126) { // 可打印字符
                    *ptr++ = c;
                    count++;
                    putchar(c);
                }
                break;
        }
    }
    
    // 防止缓冲区溢出
    *ptr = '\0';
}

static int parse_command(char *cmd_str, char **argv, char delimiter)
{
    int argc = 0;
    char *token = cmd_str;
    char *end;
    
    // 清空参数数组
    for (int i = 0; i < MAX_ARG_NUM; i++) {
        argv[i] = NULL;
    }
    
    while (*token && argc < MAX_ARG_NUM - 1) {
        // 跳过前导分隔符
        while (*token == delimiter) token++;
        if (*token == '\0') break;
        
        // 处理带引号的参数
        if (*token == '"') {
            token++; // 跳过开头的引号
            argv[argc] = token;
            
            // 查找结束引号
            end = strchr(token, '"');
            if (end) {
                *end = '\0';
                token = end + 1;
            } else {
                // 没有结束引号，使用整个字符串
                token += strlen(token);
            }
        } else {
            // 普通参数
            argv[argc] = token;
            
            // 查找下一个分隔符
            end = strchr(token, delimiter);
            if (end) {
                *end = '\0';
                token = end + 1;
            } else {
                token += strlen(token);
            }
        }
        
        argc++;
    }
    
    return argc;
}


// RTC 时间获取函数
static void get_rtc_time(char *buf, unsigned int size)
{
    // 简化的实现，实际应根据硬件实现
    current_time_t ctime;
    get_current_time(&ctime);
    printk("%d/%d/%d %d:%d:%d", ctime.year, ctime.month, ctime.day, ctime.hour, ctime.min, ctime.sec);

    //data("Y-m-d H:i:s");
}

// RTC 日期获取函数
static void get_rtc_date(char *buf, unsigned int size)
{
    // 简化的实现，实际应根据硬件实现
    current_time_t ctime;
    get_current_time(&ctime);
    printk("%d/%d/%d %d:%d:%d", ctime.year, ctime.month, ctime.day, ctime.hour, ctime.min, ctime.sec);

    //data("Y-m-d");
}



void shell_main()
{
    // 显示欢迎信息
    char welcome_msg[128];
    char date_buf[16];
    char time_buf[16];
    
    get_rtc_date(date_buf, sizeof(date_buf));
    get_rtc_time(time_buf, sizeof(time_buf));
    
    snprintf(welcome_msg, sizeof(welcome_msg), 
             "MoniOS v0.1.0 (Build %s %s) [32-bit]\n", 
             date_buf, time_buf);
             
    puts(welcome_msg);
    puts("Type 'help' for available commands\n");
    
    // 主命令循环
    while (1) {
        display_prompt();
        
        // 读取命令
        memset(shell_state.input_buffer, 0, sizeof(shell_state.input_buffer));
        read_command_line(shell_state.input_buffer, sizeof(shell_state.input_buffer));
        
        // 跳过空行
        if (shell_state.input_buffer[0] == '\0') continue;
        
        // 备份原始命令
        strncpy(shell_state.backup_buffer, shell_state.input_buffer, 
               sizeof(shell_state.backup_buffer));
        
        // 解析命令
        shell_state.argc = parse_command(shell_state.input_buffer, 
                                       shell_state.arguments, ' ');
        
        // 执行命令
        if (shell_state.argc > 0) {
            execute_command(shell_state.argc, shell_state.arguments);
        }
    }
}

// ==================== 内核主函数 ====================
void kernel_main()
{   
    
        // 初始化硬件和核心组件
    monitor_clear();
    monitor_printf("Initializing kernel...\n");
    
    // 首先初始化中断控制器
    //init_pic();
    
    init_gdtidt();
    init_memory();
    init_timer(100); // 100 Hz 定时器
    init_keyboard();
    
    // 初始化网络，但不接触硬件
    init_network_c();

    dma_init();

    sb16_init();
    
    // 启用中断
    asm volatile("sti");
    
    // 初始化任务系统
    task_init();
    monitor_printf("Task system initialized\n");
    
    // 创建 shell 任务
    /* task_t *shell_task = create_kernel_task(shell_main, 2); // 用户级任务
    if (!shell_task) {
        monitor_printf("Failed to create shell task!\n");
        return NULL;
    }
    
    // 启动任务调度
    task_start(shell_task); */
    shell_main();
    
    // 在系统完全启动后，再初始化网络硬件
    //monitor_printf("System startup complete. Initializing network hardware...\n");
    //late_init_network();
    
    // 如果需要网络功能，再启用网络中断
    // enable_network_interrupts();
    
    // 不应到达此处
    monitor_printf("Kernel main exited unexpectedly!\n");
    while (1);

}
