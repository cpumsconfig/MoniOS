#include "acpi.h"
#include "mtask.h"
#include "string.h"
#include "stdint.h"
#include "common.h"

// ACPI全局变量定义
const ACPIFadt *acpiFadt = NULL;
const ACPIHeader *acpiDsdt = NULL;
const ACPIHeader *acpiSsdt = NULL;

void *pa2va(unsigned long physAddr);

static int parseDT(ACPIHeader *dt)
{
   u32 signature = dt->signature;
   char signatureString[5];
   memcpy((void *)signatureString, (const void *)&signature, 4);
   signatureString[4] = '\0';
   printk("Found device %s from ACPI.\n", signatureString);
 
   if (signature == *(u32 *)"APIC") {
      parseApic((ACPIHeaderApic *)dt);
   } else if (signature == *(u32 *)"HPET") {
      parseHpet((ACPIHeaderHpet *)dt);
   } else if (signature == *(u32 *)"FACP") {
      acpiEnable(dt, 0); // FADT
   } else if (signature == *(u32 *)"SSDT") {
      acpiEnable(0, dt); // SSDT
   }
   return 0;
}

int acpiEnable(const void *fadt, const void *ssdt)
{
   // 设置SSDT（如果提供且尚未设置）
   if (!acpiSsdt && ssdt) {
      acpiSsdt = (const ACPIHeader *)ssdt;
   }
   
   // 如果FADT已设置，直接返回
   if (acpiFadt) {
      return 0;
   }
   
   // 检查是否提供了有效的FADT
   if (!fadt) {
      return -1;
   }
   
   // 设置FADT和DSDT
   acpiFadt = (const ACPIFadt *)fadt;
   acpiDsdt = (const ACPIHeader *)pa2va(acpiFadt->dsdt);
   
   // 如果存在SMI命令端口，尝试启用ACPI
   if (acpiFadt->smiCommandPort && 
      (acpiFadt->acpiEnable || acpiFadt->acpiDisable)) {
      outb(acpiFadt->smiCommandPort, acpiFadt->acpiEnable);
      
      // 等待主控制寄存器启用
      while (!(inw(acpiFadt->pm1aCntBlk) & SCI_ENABLED)) {
          asm volatile("pause");
      }
      
      // 如果存在辅控制寄存器，等待其启用
      if (acpiFadt->pm1bCntBlk) {
          while (!(inw(acpiFadt->pm1bCntBlk) & SCI_ENABLED)) {
              asm volatile("pause");
          }
      }
   }
   return 0;
}

// ==================== 新增关机方法 ====================

// 模拟器专用关机 (QEMU/Bochs/VirtualBox)
static void tryEmulatorPowerOff(void) {
    // QEMU 关机 (i440FX芯片组)
    outw(0x604, 0x2000);
    
    // Bochs 关机
    outw(0xB004, 0x3400);
    
    // VirtualBox 关机
    outw(0x4004, 0x3400);
    
    // VMware 关机 (使用端口0x3804)
    outw(0x3804, 0x2000);
    
    // VMware 关机 (使用端口0x4004)
    outw(0x4004, 0x3400);
    
    // VMware SMBIOS 关机
    outl(0xB2, 0x01);
    
    // QEMU isa-debug-exit 设备
    outw(0x501, 0x31);
    
    // 添加延迟
    for (volatile int i = 0; i < 1000000; i++);
}



// APM (高级电源管理) 关机
static void tryApmPowerOff(void) {
    // 检查APM BIOS签名
    if (inw(0x1004) != 'PM') {
        return; // BIOS未安装APM
    }
    
    // 连接APM接口
    outw(0x1002, 0x5300); // APM连接
    outw(0x1002, 0x5301); // 设置APM版本
    
    // 尝试关机
    outw(0x1002, 0x5307); // 设置电源状态
    outw(0x1002, 0x03);   // 关机状态
    outw(0x1002, 0x530F); // 执行关机
    
    // 添加延迟
    for (volatile int i = 0; i < 1000000; i++);
}


// PS/2键盘控制器关机
static void tryKbcPowerOff(void) {
    uint8_t status;
    
    // 等待输入缓冲区空闲
    int timeout = 100000;
    while (timeout-- > 0 && (status = inb(0x64)) & 0x02) {
        if (status & 0x01) inb(0x60); // 读取丢弃数据
        for (volatile int i = 0; i < 1000; i++); // 短延迟
    }
    
    if (timeout <= 0) return;
    
    // 发送关机命令
    outb(0x64, 0xD1);
    
    // 再次等待
    timeout = 100000;
    while (timeout-- > 0 && (inb(0x64) & 0x02)) {
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (timeout <= 0) return;
    
    // 写入关机指令
    outb(0x60, 0xFE);
    
    // 添加延迟
    for (volatile int i = 0; i < 1000000; i++);
}

// ==================== 原有关机函数 ====================

int doPowerOff(void)
{
   closeInterrupt();
   const ACPIHeader *dt = acpiDsdt; // 先尝试DSDT
   
   // 优先尝试各种备选关机方法（立即尝试）
   tryEmulatorPowerOff();
   tryApmPowerOff();
   tryKbcPowerOff();
   
   // 如果ACPI未初始化，直接返回
   if (!acpiFadt || !dt) {
      printf("ACPI not initialized. Using fallback shutdown.\n");
      // 再次尝试备选方法
      tryEmulatorPowerOff();
      tryApmPowerOff();
      tryKbcPowerOff();
      printf("ERROR:NOTTTTTTT A POWEROFF!!!!!!!!");
      for(;;); // 挂起系统
   }
   
retry:;
   u8 *data = (u8 *)dt + sizeof(ACPIHeader);
   u32 length = dt->length - sizeof(ACPIHeader);
   u8 *end = data + length;
   u8 *found = NULL;
   
   // 安全查找_S5_签名
   for (; data < end - 4; ++data) {
      if (memcmp(data, "_S5_", 4) == 0) {
         // 检查前导字节格式
         if (data > (u8 *)dt + sizeof(ACPIHeader)) {
            if (data[-1] == 0x08 ||  // NameOp前缀
                (data[-1] == '\\' && data[-2] == 0x08)) { // 带前缀的NameOp
               found = data;
               break;
            }
         }
      }
   }
   
   // 处理查找失败情况
   if (!found) {
      if (dt != acpiSsdt && acpiSsdt) {
         dt = acpiSsdt;
         goto retry;
      }
      printf("ACPI _S5_ not found. Using fallback shutdown.\n");
      // 再次尝试备选方法
      tryEmulatorPowerOff();
      tryApmPowerOff();
      tryKbcPowerOff();
      for(;;); // 挂起系统
   }
   
   // 解析_S5_包结构
   data = found + 5;
   if (data >= end) goto failure;
   
   // 解析包前缀
   u8 pkg_offset = ((*data) & 0xC0) >> 6;
   data += pkg_offset + 2;
   if (data >= end) goto failure;
   
   // 提取SLP_TYPa和SLP_TYPb值
   u16 s5a = 0, s5b = 0;
   
   // 处理可能的BytePrefix
   if (*data == 0x0A) data++;
   s5a = (*data++) << 10;
   
   if (*data == 0x0A) data++;
   s5b = (*data++) << 10;
   
   // 发送关机命令（带延迟）
   for (int i = 0; i < 5; i++) { // 尝试5次
      outw(acpiFadt->pm1aCntBlk, s5a | SLP_EN);
      if (acpiFadt->pm1bCntBlk) {
         outw(acpiFadt->pm1bCntBlk, s5b | SLP_EN);
      }
      
      // 添加延迟等待关机生效
      for (volatile int j = 0; j < 10000000; j++);
   }
   
failure:
   // 如果ACPI关机失败，尝试备选方法
   printf("ACPI shutdown failed. Using fallback methods.\n");
   tryEmulatorPowerOff();
   tryApmPowerOff();
   tryKbcPowerOff();
   
   // 所有方法都失败，死循环
   printf("ERROR:NO SHUTDOWN METHOD FOUND!");
   for(;;);
   return -1;
}

static void tryKbcReboot(void) {
    int timeout = 100000; // 超时计数器
    
    // 尝试多次（增加成功率）
    for (int attempt = 0; attempt < 5; attempt++) {
        // 等待输入缓冲区空闲
        while (timeout-- > 0 && (inb(0x64) & 0x02)) {
            // 如有数据，读取丢弃
            if (inb(0x64) & 0x01) {
                inb(0x60);
            }
            // 短延迟
            for (volatile int i = 0; i < 1000; i++);
        }
        
        // 超时则放弃本次尝试
        if (timeout <= 0) return;
        
        // 发送复位命令
        outb(0x64, 0xFE);
        
        // 等待复位生效（长延迟）
        for (volatile int i = 0; i < 10000000; i++);
    }
}

// ACPI复位寄存器方式
static void tryAcpiReboot(void) {
    // 检查FADT是否支持复位寄存器
    if (!acpiFadt || acpiFadt->header.length < 116) return;
    
    // 直接访问内存中的复位寄存器信息
    struct {
        u8 addressSpaceId;
        u8 bitWidth;
        u8 bitOffset;
        u8 accessSize;
        u64 address;
    } *resetReg = (void *)((u8 *)acpiFadt + 116);
    
    u8 *resetValuePtr = (u8 *)acpiFadt + 116 + sizeof(*resetReg);
    u8 resetValue = *resetValuePtr;
    
    if (resetReg->address) {
        // 尝试多次发送复位命令
        for (int i = 0; i < 5; i++) {
            switch (resetReg->addressSpaceId) {
                case 1: // 系统I/O空间
                    outb((u16)resetReg->address, resetValue);
                    break;
                case 0: // 系统内存空间
                case 3: // PCI配置空间
                    // 内存映射I/O写入
                    volatile u8 *memAddr = (volatile u8 *)pa2va(resetReg->address);
                    *memAddr = resetValue;
                    break;
            }
            
            // 等待复位生效
            for (volatile int j = 0; j < 10000000; j++);
        }
    }
}

// 模拟器专用重启
static void tryEmulatorReboot(void) {
    // QEMU isa-debug-exit 设备（重启模拟）
    outw(0x501, 0x32);
    
    // 其他可能的模拟器重启端口
    outw(0xCF9, 0x0E);
    outw(0x92, 0x01);
    
    // 添加延迟
    for (volatile int i = 0; i < 10000000; i++);
}

// APM重启
static void tryApmReboot(void) {
    // 检查APM BIOS签名
    if (inw(0x1004) != 'PM') return;
    
    // 连接APM接口
    outw(0x1002, 0x5300); // APM连接
    outw(0x1002, 0x5301); // 设置APM版本
    
    // 尝试重启
    outw(0x1002, 0x5307); // 设置电源状态
    outw(0x1002, 0x03);   // 重启状态值
    outw(0x1002, 0x530F); // 执行重启
    
    // 添加延迟
    for (volatile int i = 0; i < 10000000; i++);
}

int doReboot(void)
{
   closeInterrupt();
   
   // 尝试传统复位方式（键盘控制器）作为首选
   tryKbcReboot();
   
   // 尝试ACPI复位寄存器方式
   tryAcpiReboot();
   
   // 尝试多种复位方法
   tryEmulatorReboot();
   tryApmReboot();
   
   // 所有方法都失败，死循环
   printf("ERROR: NO REBOOT METHOD FOUND!");
   for(;;);
   return -1;
}




// ==================== 辅助函数 ====================

void *pa2va(unsigned long physAddr) {
   // 物理地址到虚拟地址转换
   return (void *)(physAddr + 0xFFFF800000000000); // 示例偏移
}

void closeInterrupt(void) {
   asm volatile("cli");
}

void parseApic(ACPIHeaderApic *apic)
{
    // 验证APIC表头
    if (!apic || apic->header.length < sizeof(ACPIHeaderApic)) {
        return;
    }

    // 获取APIC表中的记录
    u8 *record_ptr = (u8 *)(apic + 1);
    u8 *table_end = (u8 *)apic + apic->header.length;

    // 解析本地APIC地址
    printk("Local APIC address: 0x%08X\n", apic->localApicAddress);

    // 遍历APIC表中的所有记录
    while (record_ptr < table_end) {
        ACPIApicRecord *record = (ACPIApicRecord *)record_ptr;
        
        switch (record->type) {
            case 0: // Processor Local APIC
                {
                    ACPIApicProcessor *proc = (ACPIApicProcessor *)record;
                    printk("Processor LAPIC: ID=%d, APIC ID=%d, Flags=0x%02X\n",
                           proc->processorId, proc->apicId, proc->flags);
                }
                break;
                
            case 1: // I/O APIC
                {
                    ACPIApicIo *io = (ACPIApicIo *)record;
                    printk("I/O APIC: ID=%d, Base Address=0x%08X, GSI=%d\n",
                           io->ioApicId, io->ioApicAddress, io->globalSystemInterruptBase);
                }
                break;
                
            case 2: // Interrupt Source Override
                {
                    ACPIApicIntOverride *override = (ACPIApicIntOverride *)record;
                    printk("Int Override: Bus=%d, Source=%d, GSI=%d, Flags=0x%02X\n",
                           override->bus, override->source, override->globalSystemInterrupt, 
                           override->flags);
                }
                break;
        }
        
        record_ptr += record->length;
    }
}

void parseHpet(ACPIHeaderHpet *hpet)
{
    // 验证HPET表头
    if (!hpet || hpet->header.length < sizeof(ACPIHeaderHpet)) {
        return;
    }

    // 打印HPET基本信息
    printk("HPET Info:\n");
    printk("  Event Timer Block ID: 0x%08X\n", hpet->eventTimerBlockId);
    printk("  Base Address: 0x%016llX\n", hpet->baseAddress.address);
    printk("  HPET Number: %d\n", hpet->hpetNumber);
    printk("  Minimum Clock Tick: %d\n", hpet->minimumClockTick);
    printk("  Page Protection: %s\n", hpet->pageProtection ? "Protected" : "Unprotected");
}

// ==================== 端口I/O函数 ====================
/* 
u8 inb(u16 port) {
   u8 ret;
   asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

void outb(u16 port, u8 value) {
   asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

u16 inw(u16 port) {
   u16 ret;
   asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

void outw(u16 port, u16 value) {
   asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
*/