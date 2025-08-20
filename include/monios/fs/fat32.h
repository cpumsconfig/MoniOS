#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 文件访问模式
#define O_READ  0x01
#define O_WRITE 0x02
#define O_RDWR  (O_READ | O_WRITE)

// 文件属性
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT32常量
#define FAT32_EOC       0x0FFFFFF8     // 簇链结束标记
#define FAT32_MAX_PATH  256            // 最大路径长度
#define FAT32_MAX_NAME  13             // 8.3文件名最大长度(包括点)

// FAT32驱动
// 磁盘IO端口定义（根据实际硬件平台调整）
#define DISK_IO_BASE 0x1F0  // IDE控制器主基地址
#define DISK_ERROR   (DISK_IO_BASE + 1)
#define DISK_SECCNT  (DISK_IO_BASE + 2)
#define DISK_LBA_LO  (DISK_IO_BASE + 3)
#define DISK_LBA_MID (DISK_IO_BASE + 4)
#define DISK_LBA_HI  (DISK_IO_BASE + 5)
#define DISK_DRVHD   (DISK_IO_BASE + 6)
#define DISK_STATUS  (DISK_IO_BASE + 7)
#define DISK_COMMAND DISK_STATUS

// 磁盘命令
#define DISK_CMD_READ  0x20
#define DISK_CMD_WRITE 0x30

// 磁盘访问函数原型 (需要用户实现)
void disk_read(uint32_t sector, uint32_t count, void* buffer);
void disk_write(uint32_t sector, uint32_t count, const void* buffer);

// 调试输出函数 (需要用户实现)
void kprintf(const char* fmt, ...);

// FAT32引导扇区结构
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} FAT32_BootSector;
#pragma pack(pop)

// FAT32目录项结构
#pragma pack(push, 1)
typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} FAT32_DirEntry;
#pragma pack(pop)

// 文件句柄
typedef struct FILE {
    uint32_t start_cluster;    // 起始簇号
    uint32_t current_cluster;  // 当前簇号
    uint32_t position;         // 文件内位置 (字节)
    uint32_t size;             // 文件大小
    uint32_t dir_sector;       // 目录项所在扇区
    uint32_t dir_offset;       // 目录项在扇区内的偏移
    bool     modified;         // 文件是否被修改
    uint8_t  mode;             // 访问模式 (O_READ, O_WRITE, O_RDWR)
} FILE;

// 文件系统初始化
int fat32_init(uint32_t boot_sector);

// 文件操作
FILE* fat32_open(const char* path, uint8_t mode);
uint32_t fat32_read(FILE* file, void* buffer, uint32_t size);
uint32_t fat32_write(FILE* file, const void* buffer, uint32_t size);
void fat32_close(FILE* file);

// 目录操作
int fat32_mkdir(const char* path);
int fat32_create_file(const char* path);
void fat32_listdir(const char* path);

// 文件/目录信息
typedef struct {
    char name[FAT32_MAX_NAME]; // 文件名 (8.3格式)
    uint32_t size;             // 文件大小
    uint8_t attributes;        // 文件属性
    bool is_directory;         // 是否为目录
} DirEntryInfo;

// 目录遍历
int fat32_readdir(const char* path, DirEntryInfo* entries, int max_entries);

// 实用函数
int fat32_exists(const char* path);
int fat32_delete(const char* path);
int fat32_rename(const char* old_path, const char* new_path);

// 文件系统信息
typedef struct {
    uint32_t total_clusters;   // 总簇数
    uint32_t free_clusters;    // 空闲簇数
    uint32_t bytes_per_cluster;// 每簇字节数
    char volume_label[12];     // 卷标
} FSInfo;

int fat32_get_fs_info(FSInfo* info);

#endif // FAT32_H