#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
//#include "stdlib.h"
#include "monios/fs/fat32.h"
#include "monios/common.h"


// 字符串比较函数
int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

// 字符转大写函数
int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

// 内存分配函数（简化实现）
void *malloc(size_t size) {
    // 简化实现，实际系统中应该维护一个内存池
    static uint8_t heap[1024*1024]; // 1MB 堆空间
    static size_t heap_ptr = 0;
    
    // 对齐到4字节
    size = (size + 3) & ~3;
    
    if (heap_ptr + size > sizeof(heap)) {
        return NULL; // 堆空间不足
    }
    
    void *ptr = &heap[heap_ptr];
    heap_ptr += size;
    return ptr;
}

// 内存释放函数（简化实现）
void free(void *ptr) {
    // 简化实现，实际系统中应该实现真正的内存回收
    (void)ptr;
}



// 等待磁盘就绪
static inline void disk_wait_ready() {
    while (!(inb(DISK_STATUS) & 0x40))  // 检查DRQ位
        ;
}
// 磁盘访问接口 (需要根据实际硬件实现)
void disk_read(uint32_t sector, uint32_t count, void* buffer) {
    // 参数检查
    if (!buffer || count == 0) {
        return;
    }

    uint8_t* buf = (uint8_t*)buffer;
    
    // 等待磁盘就绪
    disk_wait_ready();
    
    // 发送参数
    outb(DISK_SECCNT, count);
    outb(DISK_LBA_LO, (uint8_t)(sector));
    outb(DISK_LBA_MID, (uint8_t)(sector >> 8));
    outb(DISK_LBA_HI, (uint8_t)(sector >> 16));
    outb(DISK_DRVHD, 0xE0 | ((sector >> 24) & 0x0F));  // LBA模式，主盘
    outb(DISK_COMMAND, DISK_CMD_READ);
    
    // 读取数据
    for (uint32_t i = 0; i < count; i++) {
        disk_wait_ready();
        
        // 读取一个扇区（512字节）
        for (int j = 0; j < 512; j++) {
            *buf++ = inb(DISK_IO_BASE);
        }
    }
}

void disk_write(uint32_t sector, uint32_t count, const void* buffer) {
    // 参数检查
    if (!buffer || count == 0) {
        return;
    }

    const uint8_t* buf = (const uint8_t*)buffer;
    
    // 等待磁盘就绪
    disk_wait_ready();
    
    // 发送参数
    outb(DISK_SECCNT, count);
    outb(DISK_LBA_LO, (uint8_t)(sector));
    outb(DISK_LBA_MID, (uint8_t)(sector >> 8));
    outb(DISK_LBA_HI, (uint8_t)(sector >> 16));
    outb(DISK_DRVHD, 0xE0 | ((sector >> 24) & 0x0F));  // LBA模式，主盘
    outb(DISK_COMMAND, DISK_CMD_WRITE);
    
    // 写入数据
    for (uint32_t i = 0; i < count; i++) {
        disk_wait_ready();
        
        // 写入一个扇区（512字节）
        for (int j = 0; j < 512; j++) {
            outb(DISK_IO_BASE, *buf++);
        }
    }
}

// 调试输出
void printf(const char* fmt, ...);

// FAT32 常量定义
#define FAT32_EOC 0x0FFFFFF8     // 簇链结束标记
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT32引导扇区
/* typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries_count;   // FAT32中应为0
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;          // FAT32中应为0
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;          // 每个FAT表扇区数
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;         // 根目录起始簇号
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

// FAT32目录项
typedef struct __attribute__((packed)) {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;   // 起始簇号高16位
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;    // 起始簇号低16位
    uint32_t file_size;
} FAT32_DirEntry; */

// 文件句柄
/* typedef struct {
    uint32_t start_cluster;    // 起始簇号
    uint32_t current_cluster;  // 当前簇号
    uint32_t position;         // 文件内位置 (字节)
    uint32_t size;             // 文件大小
    uint32_t dir_sector;       // 目录项所在扇区
    uint32_t dir_offset;       // 目录项在扇区内的偏移
    bool     modified;         // 文件是否被修改
    uint8_t  mode;             // 访问模式 (0=读, 1=写, 2=读写)
} FILE; */

// 文件系统全局状态
typedef struct {
    FAT32_BootSector bpb;      // 引导扇区参数
    uint32_t fat_start;        // FAT表起始扇区
    uint32_t data_start;       // 数据区起始扇区
    uint32_t bytes_per_cluster;// 每簇字节数
    uint32_t root_dir_cluster; // 根目录簇号
    uint32_t free_clusters;    // 空闲簇计数 (缓存)
} FAT32_FS;

FAT32_FS fs; // 全局文件系统实例
static inline uint32_t min(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}
// 初始化文件系统
int fat32_init(uint32_t boot_sector) {
    // 读取引导扇区
    disk_read(boot_sector, 1, &fs.bpb);
    
    // 验证FAT32签名
    if (fs.bpb.fs_type[0] != 'F' || fs.bpb.fs_type[1] != 'A' || 
        fs.bpb.fs_type[2] != 'T' || fs.bpb.fs_type[3] != '3' || 
        fs.bpb.fs_type[4] != '2') {
        printf("Invalid FAT32 signature\n");
        return -1;
    }
    
    // 计算关键位置
    fs.fat_start = boot_sector + fs.bpb.reserved_sectors;
    fs.data_start = fs.fat_start + (fs.bpb.num_fats * fs.bpb.fat_size_32);
    fs.bytes_per_cluster = fs.bpb.bytes_per_sector * fs.bpb.sectors_per_cluster;
    fs.root_dir_cluster = fs.bpb.root_cluster;
    
    printf("FAT32 initialized: %d clusters, %d bytes/cluster\n", 
           (fs.bpb.total_sectors_32 - (fs.data_start - boot_sector)) / fs.bpb.sectors_per_cluster,
           fs.bytes_per_cluster);
    
    return 0;
}

// 获取FAT表项
static uint32_t get_fat_entry(uint32_t cluster) {
    uint32_t fat_sector = fs.fat_start + (cluster * 4) / fs.bpb.bytes_per_sector;
    uint32_t fat_offset = (cluster * 4) % fs.bpb.bytes_per_sector;
    
    uint8_t sector[fs.bpb.bytes_per_sector];
    disk_read(fat_sector, 1, sector);
    
    uint32_t entry = *(uint32_t*)(sector + fat_offset);
    return entry & 0x0FFFFFFF; // 高4位保留
}

// 设置FAT表项
static void set_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_sector = fs.fat_start + (cluster * 4) / fs.bpb.bytes_per_sector;
    uint32_t fat_offset = (cluster * 4) % fs.bpb.bytes_per_sector;
    
    uint8_t sector[fs.bpb.bytes_per_sector];
    disk_read(fat_sector, 1, sector);
    
    // 保留高4位
    uint32_t* entry = (uint32_t*)(sector + fat_offset);
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);
    
    // 写入更新后的扇区
    disk_write(fat_sector, 1, sector);
    
    // 更新备份FAT
    for (int i = 1; i < fs.bpb.num_fats; i++) {
        disk_write(fat_sector + i * fs.bpb.fat_size_32, 1, sector);
    }
}

// 查找空闲簇
static uint32_t find_free_cluster() {
    // 简单线性搜索 (实际应使用FSInfo优化)
    uint32_t fat_sectors = fs.bpb.fat_size_32;
    uint8_t sector[fs.bpb.bytes_per_sector];
    
    for (uint32_t s = 0; s < fat_sectors; s++) {
        disk_read(fs.fat_start + s, 1, sector);
        
        for (uint32_t i = 0; i < fs.bpb.bytes_per_sector; i += 4) {
            uint32_t entry = *(uint32_t*)(sector + i) & 0x0FFFFFFF;
            uint32_t cluster = s * (fs.bpb.bytes_per_sector / 4) + i/4;
            
            // 跳过保留簇
            if (cluster < 2) continue;
            
            if (entry == 0) { // 空闲簇
                return cluster;
            }
        }
    }
    
    return 0; // 没有空闲簇
}

// 簇号转扇区号
static uint32_t cluster_to_sector(uint32_t cluster) {
    return fs.data_start + (cluster - 2) * fs.bpb.sectors_per_cluster;
}

// 读取簇数据
static void read_cluster(uint32_t cluster, void* buffer) {
    uint32_t sector = cluster_to_sector(cluster);
    disk_read(sector, fs.bpb.sectors_per_cluster, buffer);
}

// 写入簇数据
static void write_cluster(uint32_t cluster, const void* buffer) {
    uint32_t sector = cluster_to_sector(cluster);
    disk_write(sector, fs.bpb.sectors_per_cluster, buffer);
}

// 查找目录中的文件
static int find_in_directory(uint32_t dir_cluster, const char* filename, FAT32_DirEntry* entry, 
                             uint32_t* sector_out, uint32_t* offset_out) {
    uint8_t cluster_buf[fs.bytes_per_cluster];
    uint32_t current_cluster = dir_cluster;
    
    do {
        read_cluster(current_cluster, cluster_buf);
        FAT32_DirEntry* dir = (FAT32_DirEntry*)cluster_buf;
        
        for (int i = 0; i < fs.bytes_per_cluster / sizeof(FAT32_DirEntry); i++) {
            // 跳过空项
            if (dir[i].name[0] == 0x00) return -1;
            if (dir[i].name[0] == 0xE5) continue; // 删除项
            
            // 跳过长文件名项
            if (dir[i].attr == ATTR_LONG_NAME) continue;
            
            // 构造8.3格式文件名
            char short_name[12];
            memset(short_name, ' ', 11);
            memcpy(short_name, dir[i].name, 8);
            memcpy(short_name+8, dir[i].ext, 3);
            short_name[11] = 0;
            
            // 转换为大写比较
            char compare_name[12];
            strncpy(compare_name, filename, 11);
            for (char* p = compare_name; *p; p++) *p = toupper(*p);
            
            if (strncmp(short_name, compare_name, 11) == 0) {
                if (entry) *entry = dir[i];
                if (sector_out) *sector_out = cluster_to_sector(current_cluster);
                if (offset_out) *offset_out = i * sizeof(FAT32_DirEntry);
                return 0;
            }
        }
        
        // 获取下一个簇
        current_cluster = get_fat_entry(current_cluster);
    } while (current_cluster < FAT32_EOC && current_cluster >= 2);
    
    return -1; // 未找到
}

// 创建新簇链
static uint32_t create_chain(uint32_t start_cluster, uint32_t size) {
    uint32_t prev_cluster = start_cluster;
    uint32_t clusters_needed = (size + fs.bytes_per_cluster - 1) / fs.bytes_per_cluster;
    
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t new_cluster = find_free_cluster();
        if (new_cluster == 0) return 0; // 空间不足
        
        if (prev_cluster == 0) {
            // 新链开始
            prev_cluster = new_cluster;
            set_fat_entry(new_cluster, FAT32_EOC);
        } else {
            // 链接到前一个簇
            set_fat_entry(prev_cluster, new_cluster);
            set_fat_entry(new_cluster, FAT32_EOC);
            prev_cluster = new_cluster;
        }
    }
    
    return prev_cluster; // 返回链尾
}

// 创建文件或目录
int fat32_create(const char* path, bool is_dir) {
    // 分离路径和文件名 (简化版，只支持一级目录)
    char dir_path[64], filename[13];
    const char* sep = strrchr(path, '/');
    if (sep) {
        strncpy(dir_path, path, sep - path);
        dir_path[sep - path] = 0;
        strcpy(filename, sep + 1);
    } else {
        strcpy(dir_path, "/");
        strcpy(filename, path);
    }
    
    // 查找父目录
    uint32_t parent_cluster = fs.root_dir_cluster;
    if (strcmp(dir_path, "/") != 0) {
        FAT32_DirEntry dir_entry;
        if (find_in_directory(parent_cluster, dir_path, &dir_entry, NULL, NULL) != 0) {
            printf("Parent directory not found\n");
            return -1;
        }
        parent_cluster = (dir_entry.first_cluster_high << 16) | dir_entry.first_cluster_low;
    }
    
    // 检查文件是否已存在
    FAT32_DirEntry existing;
    if (find_in_directory(parent_cluster, filename, &existing, NULL, NULL) == 0) {
        printf("File already exists\n");
        return -1;
    }
    
    // 在父目录中寻找空闲目录项
    uint8_t cluster_buf[fs.bytes_per_cluster];
    uint32_t current_cluster = parent_cluster;
    uint32_t dir_sector = 0, dir_offset = 0;
    bool found = false;
    
    do {
        read_cluster(current_cluster, cluster_buf);
        FAT32_DirEntry* dir = (FAT32_DirEntry*)cluster_buf;
        
        for (int i = 0; i < fs.bytes_per_cluster / sizeof(FAT32_DirEntry); i++) {
            if (dir[i].name[0] == 0x00 || dir[i].name[0] == 0xE5) {
                dir_sector = cluster_to_sector(current_cluster);
                dir_offset = i * sizeof(FAT32_DirEntry);
                found = true;
                break;
            }
        }
        
        if (found) break;
        
        // 获取下一个簇
        uint32_t next_cluster = get_fat_entry(current_cluster);
        if (next_cluster >= FAT32_EOC) {
            // 分配新目录簇
            next_cluster = find_free_cluster();
            if (next_cluster == 0) return -1; // 空间不足
            
            set_fat_entry(current_cluster, next_cluster);
            set_fat_entry(next_cluster, FAT32_EOC);
            
            // 清空新簇
            memset(cluster_buf, 0, fs.bytes_per_cluster);
            write_cluster(next_cluster, cluster_buf);
            
            dir_sector = cluster_to_sector(next_cluster);
            dir_offset = 0;
            found = true;
            break;
        }
        
        current_cluster = next_cluster;
    } while (current_cluster < FAT32_EOC && current_cluster >= 2);
    
    if (!found) return -1;
    
    // 创建新目录项
    FAT32_DirEntry new_entry;
    memset(&new_entry, 0, sizeof(FAT32_DirEntry));
    
    // 设置文件名 (8.3格式)
    char base[9], ext[4];
    memset(base, ' ', 8);
    memset(ext, ' ', 3);
    
    char* dot = strchr(filename, '.');
    if (dot) {
        strncpy(base, filename, min(8, dot - filename));
        strncpy(ext, dot + 1, 3);
    } else {
        strncpy(base, filename, 8);
    }
    
    // 转换为大写
    for (int i = 0; i < 8; i++) base[i] = toupper(base[i]);
    for (int i = 0; i < 3; i++) ext[i] = toupper(ext[i]);
    
    memcpy(new_entry.name, base, 8);
    memcpy(new_entry.ext, ext, 3);
    
    // 设置属性
    new_entry.attr = is_dir ? ATTR_DIRECTORY : ATTR_ARCHIVE;
    
    // 设置起始簇 (目录需要至少一个簇)
    uint32_t start_cluster = is_dir ? find_free_cluster() : 0;
    if (is_dir && start_cluster == 0) return -1;
    
    new_entry.first_cluster_high = start_cluster >> 16;
    new_entry.first_cluster_low = start_cluster & 0xFFFF;
    
    // 如果是目录，初始化目录内容
    if (is_dir) {
        // 创建 "." 和 ".." 目录项
        uint8_t dir_cluster[fs.bytes_per_cluster];
        memset(dir_cluster, 0, fs.bytes_per_cluster);
        
        FAT32_DirEntry* dot_entry = (FAT32_DirEntry*)dir_cluster;
        FAT32_DirEntry* dotdot_entry = dot_entry + 1;
        
        // "." 目录项
        memset(dot_entry, 0, sizeof(FAT32_DirEntry));
        memcpy(dot_entry->name, ".       ", 8);
        dot_entry->attr = ATTR_DIRECTORY;
        dot_entry->first_cluster_high = start_cluster >> 16;
        dot_entry->first_cluster_low = start_cluster & 0xFFFF;
        
        // ".." 目录项
        memset(dotdot_entry, 0, sizeof(FAT32_DirEntry));
        memcpy(dotdot_entry->name, "..      ", 8);
        dotdot_entry->attr = ATTR_DIRECTORY;
        dotdot_entry->first_cluster_high = parent_cluster >> 16;
        dotdot_entry->first_cluster_low = parent_cluster & 0xFFFF;
        
        write_cluster(start_cluster, dir_cluster);
        set_fat_entry(start_cluster, FAT32_EOC); // 标记为簇链结束
    }
    
    // 写入目录项
    disk_read(dir_sector, 1, cluster_buf);
    memcpy(cluster_buf + dir_offset, &new_entry, sizeof(FAT32_DirEntry));
    disk_write(dir_sector, 1, cluster_buf);
    
    return 0;
}

// 打开文件
FILE* fat32_open(const char* path, uint8_t mode) {
    // 简化版：只支持根目录下的文件
    FAT32_DirEntry entry;
    uint32_t dir_sector, dir_offset;
    
    if (find_in_directory(fs.root_dir_cluster, path, &entry, &dir_sector, &dir_offset) != 0) {
        // 文件不存在，创建新文件
        if (mode & 1) { // 写模式
            if (fat32_create(path, false) != 0) return NULL;
            if (find_in_directory(fs.root_dir_cluster, path, &entry, &dir_sector, &dir_offset) != 0) {
                return NULL;
            }
        } else {
            return NULL; // 读模式但文件不存在
        }
    }
    
    // 创建文件句柄
    FILE* file = (FILE*)malloc(sizeof(FILE));
    if (!file) return NULL;
    
    file->start_cluster = (entry.first_cluster_high << 16) | entry.first_cluster_low;
    file->current_cluster = file->start_cluster;
    file->position = 0;
    file->size = entry.file_size;
    file->dir_sector = dir_sector;
    file->dir_offset = dir_offset;
    file->modified = false;
    file->mode = mode;
    
    return file;
}

// 读取文件
uint32_t fat32_read(FILE* file, void* buffer, uint32_t size) {
    if (!file || !(file->mode & 1)) return 0; // 检查读权限
    
    uint32_t bytes_read = 0;
    uint8_t* buf_ptr = (uint8_t*)buffer;
    
    while (size > 0) {
        // 计算当前簇内的偏移
        uint32_t cluster_offset = file->position % fs.bytes_per_cluster;
        uint32_t bytes_in_cluster = fs.bytes_per_cluster - cluster_offset;
        uint32_t to_read = min(size, bytes_in_cluster);
        
        // 检查是否超出文件范围
        if (file->position >= file->size) break;
        if (file->position + to_read > file->size) {
            to_read = file->size - file->position;
        }
        
        // 读取数据
        if (to_read > 0) {
            uint8_t cluster_buf[fs.bytes_per_cluster];
            read_cluster(file->current_cluster, cluster_buf);
            memcpy(buf_ptr, cluster_buf + cluster_offset, to_read);
            
            buf_ptr += to_read;
            bytes_read += to_read;
            file->position += to_read;
            size -= to_read;
        }
        
        // 移动到下一簇
        if (file->position % fs.bytes_per_cluster == 0 && size > 0) {
            uint32_t next_cluster = get_fat_entry(file->current_cluster);
            if (next_cluster >= FAT32_EOC || next_cluster < 2) break;
            file->current_cluster = next_cluster;
        }
    }
    
    return bytes_read;
}

// 写入文件
uint32_t fat32_write(FILE* file, const void* buffer, uint32_t size) {
    if (!file || !(file->mode & 2)) return 0; // 检查写权限
    
    uint32_t bytes_written = 0;
    const uint8_t* buf_ptr = (const uint8_t*)buffer;
    file->modified = true;
    
    while (size > 0) {
        // 计算当前簇内的偏移
        uint32_t cluster_offset = file->position % fs.bytes_per_cluster;
        uint32_t bytes_in_cluster = fs.bytes_per_cluster - cluster_offset;
        uint32_t to_write = min(size, bytes_in_cluster);
        
        // 需要分配新簇的情况
        if (file->position >= file->size || cluster_offset == 0) {
            // 文件末尾且当前簇已满
            if (cluster_offset == 0 && file->position >= file->size) {
                uint32_t new_cluster = find_free_cluster();
                if (new_cluster == 0) break; // 空间不足
                
                set_fat_entry(file->current_cluster, new_cluster);
                set_fat_entry(new_cluster, FAT32_EOC);
                file->current_cluster = new_cluster;
                
                // 清空新簇
                uint8_t zero[fs.bytes_per_cluster];
                memset(zero, 0, fs.bytes_per_cluster);
                write_cluster(new_cluster, zero);
            }
        }
        
        // 写入数据
        uint8_t cluster_buf[fs.bytes_per_cluster];
        read_cluster(file->current_cluster, cluster_buf);
        memcpy(cluster_buf + cluster_offset, buf_ptr, to_write);
        write_cluster(file->current_cluster, cluster_buf);
        
        buf_ptr += to_write;
        bytes_written += to_write;
        file->position += to_write;
        size -= to_write;
        
        // 更新文件大小
        if (file->position > file->size) {
            file->size = file->position;
        }
    }
    
    return bytes_written;
}

// 关闭文件
void fat32_close(FILE* file) {
    if (!file) return;
    
    // 更新目录项中的文件大小
    if (file->modified) {
        uint8_t sector[fs.bpb.bytes_per_sector];
        disk_read(file->dir_sector, 1, sector);
        
        FAT32_DirEntry* entry = (FAT32_DirEntry*)(sector + file->dir_offset);
        entry->file_size = file->size;
        
        disk_write(file->dir_sector, 1, sector);
    }
    
    free(file);
}

// 列出目录内容
void fat32_listdir(const char* path) {
    uint32_t dir_cluster = fs.root_dir_cluster;
    
    // 解析路径 (简化版)
    if (path && strcmp(path, "/") != 0) {
        FAT32_DirEntry entry;
        if (find_in_directory(dir_cluster, path, &entry, NULL, NULL) != 0) {
            printf("Directory not found\n");
            return;
        }
        dir_cluster = (entry.first_cluster_high << 16) | entry.first_cluster_low;
    }
    
    uint32_t current_cluster = dir_cluster;
    uint8_t cluster_buf[fs.bytes_per_cluster];
    
    printf("Contents of %s:\n", path ? path : "/");
    
    do {
        read_cluster(current_cluster, cluster_buf);
        FAT32_DirEntry* dir = (FAT32_DirEntry*)cluster_buf;
        
        for (int i = 0; i < fs.bytes_per_cluster / sizeof(FAT32_DirEntry); i++) {
            // 结束标记
            if (dir[i].name[0] == 0x00) return;
            
            // 跳过删除项和长文件名项
            if (dir[i].name[0] == 0xE5) continue;
            if (dir[i].attr == ATTR_LONG_NAME) continue;
            
            // 构造文件名
            char name[13];
            memcpy(name, dir[i].name, 8);
            name[8] = '.';
            memcpy(name+9, dir[i].ext, 3);
            name[12] = 0;
            
            // 去除尾部空格
            for (int j = 7; j >= 0 && name[j] == ' '; j--) name[j] = 0;
            for (int j = 11; j >= 9 && name[j] == ' '; j--) name[j] = 0;
            
            printf("%c %10d %s\n", 
                   (dir[i].attr & ATTR_DIRECTORY) ? 'D' : 'F',
                   dir[i].file_size,
                   name);
        }
        
        // 获取下一个簇
        current_cluster = get_fat_entry(current_cluster);
    } while (current_cluster < FAT32_EOC && current_cluster >= 2);
}

// 示例使用
/* void fs_test() {
    // 初始化文件系统 (假设引导扇区在LBA 0)
    if (fat32_init(0) != 0) {
        printf("FAT32 initialization failed\n");
        return;
    }
    
    // 创建新文件
    fat32_create("TEST.TXT", false);
    
    // 写入文件
    FILE* file = fat32_open("TEST.TXT", 2); // 读写模式
    if (file) {
        const char* text = "Hello FAT32!";
        fat32_write(file, text, strlen(text));
        fat32_close(file);
    }
    
    // 创建目录
    fat32_create("DOCS", true);
    
    // 列出根目录内容
    fat32_listdir("/");
    
    // 读取文件
    file = fat32_open("TEST.TXT", 0); // 只读
    if (file) {
        char buffer[256];
        uint32_t read = fat32_read(file, buffer, sizeof(buffer));
        buffer[read] = 0;
        printf("File content: %s\n", buffer);
        fat32_close(file);
    }
} */