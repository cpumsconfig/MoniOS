#ifndef _ACPI_H_
#define _ACPI_H_

#include "stdint.h"
#include "stddef.h"

// 基本类型定义
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

// ACPI表头结构
typedef struct {
    u32 signature;       // 表签名（如"FACP"）
    u32 length;          // 表总长度
    u8  revision;        // ACPI版本
    u8  checksum;        // 整表校验和
    u8  oemId[6];        // OEM标识
    u8  oemTableId[8];   // OEM表ID
    u32 oemRevision;     // OEM修订号
    u32 creatorId;       // 创建者ID
    u32 creatorRevision; // 创建者修订号
} ACPIHeader;

// APIC相关结构体定义
typedef struct {
    u8 type;
    u8 length;
} ACPIApicRecord;

typedef struct {
    ACPIApicRecord record;
    u8 processorId;
    u8 apicId;
    u32 flags;
} ACPIApicProcessor;

typedef struct {
    ACPIApicRecord record;
    u8 ioApicId;
    u8 reserved;
    u32 ioApicAddress;
    u32 globalSystemInterruptBase;
} ACPIApicIo;

typedef struct {
    ACPIApicRecord record;
    u8 bus;
    u8 source;
    u32 globalSystemInterrupt;
    u16 flags;
} ACPIApicIntOverride;

typedef struct {
    ACPIHeader header;
    u32 localApicAddress;  // 使用一致的命名
    u32 flags;
} ACPIHeaderApic;

// HPET相关结构体定义
typedef struct {
    ACPIHeader header;
    u32 eventTimerBlockId;
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } baseAddress;
    u8 hpetNumber;
    u16 minimumClockTick;
    u8 pageProtection;
    u8 reserved;  // 添加保留字段以确保正确对齐
} ACPIHeaderHpet;

// FADT结构（Fixed ACPI Description Table）
typedef struct {
    ACPIHeader header;   // 标准ACPI表头
    u32 firmwareCtrl;    // 32位FACS地址
    u32 dsdt;            // DSDT物理地址
    
    // 系统管理端口
    u8  reserved0;       // 保留字段
    u8  preferredPmProfile; // 首选电源管理配置
    u16 sciInt;          // SCI中断号
    u32 smiCommandPort;  // SMI命令端口
    u8  acpiEnable;      // 启用ACPI命令值
    u8  acpiDisable;     // 禁用ACPI命令值
    u8  S4biosReq;       // S4 BIOS请求
    u8  pstateControl;   // P状态控制
    u32 pm1aEvtBlk;      // PM1a事件块地址
    u32 pm1bEvtBlk;      // PM1b事件块地址
    u32 pm1aCntBlk;      // PM1a控制块地址
    u32 pm1bCntBlk;      // PM1b控制块地址
    u32 pm2CntBlk;       // PM2控制块地址
    u32 pmTmrBlk;        // PM定时器块地址
    u32 gpe0Blk;         // 通用事件0块地址
    u32 gpe1Blk;         // 通用事件1块地址
    u8  pm1EvtLen;       // PM1事件长度
    u8  pm1CntLen;       // PM1控制长度
    u8  pm2CntLen;       // PM2控制长度
    u8  pmTmrLen;        // PM定时器长度
    u8  gpe0BlkLen;      // 通用事件0块长度
    u8  gpe1BlkLen;      // 通用事件1块长度
    u8  gpe1Base;        // 通用事件1基偏移
    u8  cstControl;      // C状态控制
    u16 C2Latency;       // C2延迟
    u16 C3Latency;       // C3延迟
    u16 flushSize;       // 缓存刷新大小
    u16 flushStride;     // 缓存刷新步长
    u8  dutyOffset;      // 占空比偏移
    u8  dutyWidth;       // 占空比宽度
    u8  dayAlarm;        // 日报警索引
    u8  monthAlarm;      // 月报警索引
    u8  century;         // 世纪索引
    u16 bootFlags;       // 启动标志
    u8  reserved;        // 保留字段
    u32 flags;           // 标志位
    // 以下字段为ACPI 2.0+扩展
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } resetRegister;     // 复位寄存器
    u8  resetValue;      // 复位命令值
    u16 armBootFlags;    // ARM启动标志
    u8  minorRevision;   // 次要版本
    u64 XFirmwareCtrl;   // 64位FACS地址
    u64 XDsdt;           // 64位DSDT地址
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xpm1aEvtBlk;       // 扩展PM1a事件块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xpm1bEvtBlk;       // 扩展PM1b事件块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xpm1aCntBlk;       // 扩展PM1a控制块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xpm1bCntBlk;       // 扩展PM1b控制块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xpm2CntBlk;        // 扩展PM2控制块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } XPmTmrBlk;         // 扩展PM定时器块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xgpe0Blk;          // 扩展通用事件0块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } Xgpe1Blk;          // 扩展通用事件1块
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } sleepControlReg;   // 睡眠控制寄存器
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } sleepStatusReg;    // 睡眠状态寄存器
    u64 hypervisorVendorIdentity; // 虚拟机厂商标识
} ACPIFadt;

// ACPI全局变量
extern const ACPIFadt *acpiFadt;
extern const ACPIHeader *acpiDsdt;
extern const ACPIHeader *acpiSsdt;

// ACPI寄存器状态标志
#define SCI_ENABLED  0x0001  // ACPI使能标志位
#define SLP_EN       0x2000  // SLP_EN使能位(1<<13)

#endif // _ACPI_H_
