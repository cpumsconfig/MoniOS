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
#include "screen.h"

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

static char current_path[256] = "/";

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

int cmd_cat(const char *filename) {
    int fd = sys_open((char *)filename, O_RDONLY);
    if (fd < 0) {
        print_str("Error: Cannot open file ");
        print_str(filename);
        print_str("\n");
        return -1;
    }
    
    char buffer[1024];
    int bytes_read;
    
    while ((bytes_read = sys_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0'; // 确保字符串终止
        print_str(buffer);
    }
    
    sys_close(fd);
    print_str("\n");
    return 0;
}

// 当前工作目录
// 获取当前工作目录
const char* get_current_path(void) {
    return current_path;
}


// 实现ls命令

// 实现ls命令
// 实现ls命令
// 实现ls命令
int cmd_ls(const char *path) {
    char target_path[256];
    
    if (path == NULL || strcmp(path, "") == 0) {
        // 如果没有指定路径，使用当前路径
        strcpy(target_path, current_path);
    } else if (path[0] == '/') {
        // 绝对路径
        strcpy(target_path, path);
    } else {
        // 相对路径
        strcpy(target_path, current_path);
        strcat(target_path, path);
    }
    
    // 确保路径以/结尾
    int len = strlen(target_path);
    if (len > 0 && target_path[len - 1] != '/') {
        target_path[len] = '/';
        target_path[len + 1] = '\0';
    }
    
    // 如果是根目录，直接读取根目录项
    if (strcmp(target_path, "/") == 0) {
        int entries;
        fileinfo_t *root_dir = read_dir_entries(&entries);
        
        for (int i = 0; i < entries; i++) {
            if (root_dir[i].name[0] == 0) {
                break; // 到达目录末尾
            }
            
            if (root_dir[i].name[0] == 0xe5) {
                continue; // 跳过已删除项
            }
            
            // 显示文件名
            char filename[13]; // 8 + 1 + 3 + 1
            memset(filename, 0, sizeof(filename));
            
            // 复制文件名（去除末尾空格）
            int j;
            for (j = 0; j < 8 && root_dir[i].name[j] != ' '; j++) {
                filename[j] = root_dir[i].name[j];
            }
            
            // 添加扩展名（如果有）
            int has_ext = 0;
            for (int k = 0; k < 3; k++) {
                if (root_dir[i].ext[k] != ' ') {
                    has_ext = 1;
                    break;
                }
            }
            
            if (has_ext) {
                filename[j++] = '.';
                for (int k = 0; k < 3 && root_dir[i].ext[k] != ' '; k++) {
                    filename[j++] = root_dir[i].ext[k];
                }
            }
            
            filename[j] = '\0';
            
            // 如果是目录，添加/
            if (root_dir[i].type == 0x10) {
                filename[j++] = '/';
                filename[j] = '\0';
            }
            
            print_str(filename);
            print_str("\n");
        }
        
        kfree(root_dir);
        return 0;
    }
    
    // 非根目录处理
    // 提取目录名
    char dirname[12];
    char *last_slash = strrchr(target_path, '/');
    
    // 特殊处理：如果路径以/结尾，我们需要找到前一个斜杠
    if (last_slash != NULL && last_slash[1] == '\0') {
        // 找到前一个斜杠
        char *prev_slash = last_slash - 1;
        while (prev_slash >= target_path && *prev_slash != '/') {
            prev_slash--;
        }
        
        if (prev_slash >= target_path) {
            // 找到了前一个斜杠
            char *name_start = prev_slash + 1;
            int name_len = last_slash - name_start;
            
            // 限制长度
            if (name_len > 11) name_len = 11;
            
            // 复制目录名
            memcpy(dirname, name_start, name_len);
            dirname[name_len] = '\0';
        } else {
            // 没有找到前一个斜杠，可能是根目录下的目录
            if (strlen(target_path) > 1) {
                // 复制第一个字符后的内容
                memcpy(dirname, target_path + 1, strlen(target_path) - 2);
                dirname[strlen(target_path) - 2] = '\0';
            } else {
                // 只有根目录，不应该到这里
                strcpy(dirname, "");
            }
        }
    } else if (last_slash != NULL) {
        // 正常情况：斜杠不在末尾
        char *name_start = last_slash + 1;
        int name_len = strlen(name_start);
        
        // 限制长度
        if (name_len > 11) name_len = 11;
        
        // 复制目录名
        memcpy(dirname, name_start, name_len);
        dirname[name_len] = '\0';
    } else {
        // 没有斜杠
        strcpy(dirname, target_path);
        if (strlen(dirname) > 11) {
            dirname[11] = '\0';
        }
    }
    
    // 检查目录名是否为空
    if (strlen(dirname) == 0) {
        print_str("Error: Empty directory name\n");
        return -1;
    }
    
    // 尝试打开目录
    fileinfo_t finfo;
    if (fat16_open_file(&finfo, dirname) == 0) {
        // 检查是否是目录
        if (finfo.type == 0x10) { // 目录类型
            // 读取目录内容
            // 注意：这里简化处理，假设目录内容就是文件内容
            char *content = (char *) kmalloc(finfo.size + 1);
            if (content == NULL) {
                print_str("Error: Out of memory\n");
                return -1;
            }
            
            if (fat16_read_file(&finfo, content) == 0) {
                // 简单解析：假设每行一个文件名
                char *line = content;
                while (*line != '\0') {
                    char *next_line = strchr(line, '\n');
                    if (next_line != NULL) {
                        *next_line = '\0';
                        next_line++;
                    }
                    
                    // 跳过空行
                    if (*line != '\0') {
                        print_str(line);
                        print_str("\n");
                    }
                    
                    line = next_line;
                    if (line == NULL) break;
                }
            }
            
            kfree(content);
            return 0;
        } else {
            print_str("Error: Not a directory: ");
            print_str(dirname);
            print_str("\n");
            return -1;
        }
    } else {
        print_str("Error: Directory not found: ");
        print_str(dirname);
        print_str("\n");
        return -1;
    }
}



// 实现cd命令
int cmd_cd(const char *path) {
    if (path == NULL || strcmp(path, "") == 0) {
        print_str("Usage: cd <directory>\n");
        return -1;
    }
    
    // 调试信息：显示输入路径
    print_str("Debug: Input path = '");
    print_str(path);
    print_str("'\n");
    
    if (strcmp(path, "..") == 0) {
        // 处理上级目录
        int len = strlen(current_path);
        if (len <= 1) return 0; // 已经是根目录
        
        // 找到最后一个斜杠
        int i;
        for (i = len - 2; i >= 0; i--) {
            if (current_path[i] == '/') break;
        }
        
        if (i < 0) {
            // 没有找到斜杠，回到根目录
            strcpy(current_path, "/");
        } else {
            // 截断到最后一个斜杠
            current_path[i + 1] = '\0';
        }
        
        print_str("Debug: Changed to parent directory, current_path = '");
        print_str(current_path);
        print_str("'\n");
        
        return 0;
    } else if (strcmp(path, ".") == 0) {
        // 当前目录，不做任何事
        return 0;
    }
    
    // 处理普通目录
    char new_path[256];
    
    if (path[0] == '/') {
        // 绝对路径
        strcpy(new_path, path);
    } else {
        // 相对路径
        strcpy(new_path, current_path);
        // 确保当前路径以/结尾
        int len = strlen(new_path);
        if (len > 0 && new_path[len - 1] != '/') {
            new_path[len] = '/';
            new_path[len + 1] = '\0';
        }
        strcat(new_path, path);
    }
    
    // 确保路径以/结尾
    int len = strlen(new_path);
    if (len > 0 && new_path[len - 1] != '/') {
        new_path[len] = '/';
        new_path[len + 1] = '\0';
    }
    
    // 调试信息：显示构造的路径
    print_str("Debug: Constructed path = '");
    print_str(new_path);
    print_str("'\n");
    
    // 提取目录名
    char dirname[12]; // 8.3格式，最多11个字符+1个空字符
    char *last_slash = strrchr(new_path, '/');
    
    // 特殊处理：如果路径以/结尾，我们需要找到前一个斜杠
    if (last_slash != NULL && last_slash[1] == '\0') {
        // 找到前一个斜杠
        char *prev_slash = last_slash - 1;
        while (prev_slash >= new_path && *prev_slash != '/') {
            prev_slash--;
        }
        
        if (prev_slash >= new_path) {
            // 找到了前一个斜杠
            char *name_start = prev_slash + 1;
            int name_len = last_slash - name_start;
            
            // 限制长度
            if (name_len > 11) name_len = 11;
            
            // 复制目录名
            memcpy(dirname, name_start, name_len);
            dirname[name_len] = '\0';
        } else {
            // 没有找到前一个斜杠，可能是根目录下的目录
            if (strlen(new_path) > 1) {
                // 复制第一个字符后的内容
                memcpy(dirname, new_path + 1, strlen(new_path) - 2);
                dirname[strlen(new_path) - 2] = '\0';
            } else {
                // 只有根目录
                strcpy(dirname, "");
            }
        }
    } else if (last_slash != NULL) {
        // 正常情况：斜杠不在末尾
        char *name_start = last_slash + 1;
        int name_len = strlen(name_start);
        
        // 限制长度
        if (name_len > 11) name_len = 11;
        
        // 复制目录名
        memcpy(dirname, name_start, name_len);
        dirname[name_len] = '\0';
    } else {
        // 没有斜杠
        strcpy(dirname, new_path);
        if (strlen(dirname) > 11) {
            dirname[11] = '\0';
        }
    }
    
    // 调试信息：显示提取的目录名
    print_str("Debug: Extracted dirname = '");
    print_str(dirname);
    print_str("'\n");
    
    // 检查目录名是否为空
    if (strlen(dirname) == 0) {
        print_str("Error: Empty directory name\n");
        return -1;
    }
    
    // 尝试打开目录
    fileinfo_t finfo;
    if (fat16_open_file(&finfo, dirname) == 0) {
        // 检查是否是目录
        if (finfo.type == 0x10) { // 目录类型
            // 更新当前路径
            strcpy(current_path, new_path);
            
            print_str("Debug: Successfully changed directory\n");
            return 0;
        } else {
            print_str("Error: Not a directory: ");
            print_str(dirname);
            print_str("\n");
            return -1;
        }
    } else {
        print_str("Error: Directory not found: ");
        print_str(dirname);
        print_str("\n");
        return -1;
    }
}

// 实现mkdir命令
int cmd_mkdir(const char *path) {
    char target_path[256];
    
    if (path == NULL || strcmp(path, "") == 0) {
        print_str("Usage: mkdir <directory>\n");
        return -1;
    }
    
    if (path[0] == '/') {
        // 绝对路径
        strcpy(target_path, path);
    } else {
        // 相对路径
        strcpy(target_path, current_path);
        strcat(target_path, path);
    }
    
    // 确保路径不以/结尾（目录名不应该以/结尾）
    int len = strlen(target_path);
    if (len > 0 && target_path[len - 1] == '/') {
        target_path[len - 1] = '\0';
    }
    
    // 提取目录名
    char dirname[256];
    char *last_slash = strrchr(target_path, '/');
    if (last_slash == NULL) {
        // 没有斜杠，直接使用整个路径作为目录名
        strcpy(dirname, target_path);
    } else {
        // 有斜杠，使用最后一部分作为目录名
        strcpy(dirname, last_slash + 1);
    }
    
    // 检查目录名是否有效
    if (strlen(dirname) == 0) {
        print_str("Error: Invalid directory name\n");
        return -1;
    }
    
    // 创建目录
    fileinfo_t finfo;
    if (fat16_create_file(&finfo, dirname) == -1) {
        print_str("Error: Cannot create directory ");
        print_str(dirname);
        print_str("\n");
        return -1;
    }
    
    // 设置目录属性
    finfo.type = 0x10; // 设置为目录类型
    
    // 更新目录项
    int entries;
    fileinfo_t *root_dir = read_dir_entries(&entries);
    for (int i = 0; i < entries; i++) {
        if (!memcmp(root_dir[i].name, finfo.name, 8) && !memcmp(root_dir[i].ext, finfo.ext, 3)) {
            root_dir[i] = finfo;
            break;
        }
    }
    hd_write(ROOT_DIR_START_LBA, ROOT_DIR_SECTORS, root_dir);
    kfree(root_dir);
    
    return 0;
}

// 实现rm命令
int cmd_rm(const char *path) {
    char target_path[256];
    
    if (path == NULL || strcmp(path, "") == 0) {
        print_str("Usage: rm <file or directory>\n");
        return -1;
    }
    
    if (path[0] == '/') {
        // 绝对路径
        strcpy(target_path, path);
    } else {
        // 相对路径
        strcpy(target_path, current_path);
        strcat(target_path, path);
    }
    
    // 确保路径不以/结尾
    int len = strlen(target_path);
    if (len > 0 && target_path[len - 1] == '/') {
        target_path[len - 1] = '\0';
    }
    
    // 提取文件名或目录名
    char name[256];
    char *last_slash = strrchr(target_path, '/');
    if (last_slash == NULL) {
        // 没有斜杠，直接使用整个路径作为名称
        strcpy(name, target_path);
    } else {
        // 有斜杠，使用最后一部分作为名称
        strcpy(name, last_slash + 1);
    }
    
    // 检查名称是否有效
    if (strlen(name) == 0) {
        print_str("Error: Invalid name\n");
        return -1;
    }
    
    // 删除文件或目录
    if (fat16_delete_file(name) == -1) {
        print_str("Error: Cannot delete ");
        print_str(name);
        print_str("\n");
        return -1;
    }
    
    print_str("Deleted: ");
    print_str(name);
    print_str("\n");
    
    return 0;
}

void demo() {
    /* vga_set_mode_13h();      // 切换到图形模式
    vga_clear_screen(VGA_BLUE); // 蓝色背景 */

    uint8_t audio_data[8192]; // 8KB 音频数据
    sb16_play(audio_data, sizeof(audio_data), 22050);

    sb16_stop();

}

// 内置命令处理
static void handle_internal_command(int argc, char **argv)
{
    const char *cmd = argv[0];
    
    if (strcmp(cmd, "ver") == 0) {
        puts("MoniOS Indev\nVersion 0.1.0\nBuild: 20230810");
    } 
    else if (strcmp(cmd, "time") == 0) {
        char time_buf[32];
        get_rtc_time(time_buf, sizeof(time_buf));
        printf("Current time: %s\n", time_buf);
    } 
    else if (strcmp(cmd, "clear") == 0 || strcmp(cmd,"cls") == 0) {
        monitor_clear();
    } 
    else if (strcmp(cmd, "help") == 0) {
        puts("Available commands: ver, time, clear, help, echo, shutdown");
    } 
    else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            printf("%s ", argv[i]);
        }
        printf("\n");
        }else if (strcmp(cmd, "shutdown") == 0) {
        //取后面参数
        if (argc > 1) {
            if (strcmp(argv[1], "poweroff") == 0) {
                doPowerOff();
            } else if (strcmp(argv[1], "reboot") == 0) {
                doReboot();
            } else if (strcmp(argv[1], "help") == 0) {
                printf("Usage: shutdown [poweroff|reboot|help]\n");
            } else {
                printf("Unknown shutdown command: %s\n", argv[1]);
            }
        } else {
            printf("Usage: shutdown [poweroff|reboot|help]\n");
        }
    
    } else if (strcmp(cmd, "ping") == 0) {
        if (argc > 1) {
            printf("Pinging %s...\n", argv[1]);
            do_ping_impl_c(argv[1]);
        } else {
            printf("Usage: ping <IP address>\n");
        }
    } else if (strcmp(cmd, "netinit") == 0) {
        init_network_c();
        //late_init_network();
        if(argc > 1 && strcmp(argv[1], "on") == 0) {
            //enable_network_interrupts();
            
            printf("Network initialized.\n");
        }
    }else if(strcmp(cmd, "cat") == 0){
        if(argc > 1){
            cmd_cat(argv[1]);
        }
        else{
            printf("Usage: cat <file>\n");
        }

    }else if(strcmp(cmd, "ls") == 0){
        if(argc > 1){
            cmd_ls(argv[1]);
        }
        else{
            cmd_ls(current_path);
        }
    }else if(strcmp(cmd, "cd") == 0){
        if(argc > 1){
            cmd_cd(argv[1]);
        }else{
            printf("Usage: cd <directory>\n");
        }
    } else if(strcmp(cmd, "mkdir") == 0) {
        if(argc > 1) {
            cmd_mkdir(argv[1]);
        } else {
            printf("Usage: mkdir <directory>\n");
        }
    }else if(strcmp(cmd, "rm") == 0) {
        if(argc > 1) {
            cmd_rm(argv[1]);
        } else {
            printf("Usage: rm <file or directory>\n");
        }
    }else if(strcmp(cmd, "demo") == 0) {
        //call_bios_int();
        //set_vga_mode();
        demo();
        while (1);

    }
    else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for a list of available commands.\n");
    }



    
}
static int execute_external_command(const char *name)
{
    int pid = create_process(name, shell_state.backup_buffer, "/");
    
    if (pid == -1) {
        // 尝试添加 .bin 扩展名
        char extended_name[MAX_CMD_LEN];
        strcpy(extended_name, name);
        unsigned int len = strlen(name);
        extended_name[len] = '.';
        extended_name[len+1] = 'b';
        extended_name[len+2] = 'i';
        extended_name[len+3] = 'n';
        extended_name[len+4] = '\0';
        
        pid = create_process(extended_name, shell_state.backup_buffer, "/");
        if (pid == -1) return -1; // 命令不存在
    }
    
    int status = waitpid(pid);
    return status;
}


void execute_command(int argc, char **argv)
{
    if (argc == 0) return;
    
    // 检查内置命令
        // 检查内置命令
    if (strcmp(argv[0], "ver") == 0 || 
        strcmp(argv[0], "time") == 0 || 
        strcmp(argv[0], "clear") == 0 || 
        strcmp(argv[0], "help") == 0 ||
        strcmp(argv[0], "echo") == 0 ||
        strcmp(argv[0], "shutdown") == 0 ||
        strcmp(argv[0], "ping") == 0 ||
        strcmp(argv[0], "netinit") == 0 ||
        strcmp(argv[0], "cat") == 0 ||
        strcmp(argv[0], "ls") == 0 ||
        strcmp(argv[0], "cd") == 0 ||
        strcmp(argv[0], "mkdir") == 0 ||
        strcmp(argv[0], "rm") == 0 ||
        strcmp(argv[0], "demo") == 0 ||
        strcmp(argv[0], "cls") == 0){
        handle_internal_command(argc, argv);
        return;
    }

    
    // 尝试执行外部命令
    int status = execute_external_command(argv[0]);
    
    if (status == -1) {
        printf("Command not found: %s\n", argv[0]);
    } else if (status != 0) {
        printf("Command '%s' failed with exit code: %d\n", argv[0], status);
    }

}
