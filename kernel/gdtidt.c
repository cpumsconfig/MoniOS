#include "common.h"
#include "gdtidt.h"
#include "log.h"

extern void gdt_flush(uint32_t);
extern void idt_flush(uint32_t);
extern void syscall_handler();

gdt_entry_t gdt_entries[4096];
gdt_ptr_t gdt_ptr;
idt_entry_t idt_entries[256];
idt_ptr_t idt_ptr;

void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint16_t ar)
{
    if (limit > 0xfffff) {
        ar |= 0x8000;
        limit /= 0x1000;
    }
    gdt_entries[num].base_low = base & 0xFFFF;
    gdt_entries[num].base_mid = (base >> 16) & 0xFF;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low = limit & 0xFFFF;
    gdt_entries[num].limit_high = ((limit >> 16) & 0x0F) | ((ar >> 8) & 0xF0);
    gdt_entries[num].access_right = ar & 0xFF;
}

static void init_gdt()
{   
    //printf_info("Start GDT");
    gdt_ptr.limit = sizeof(gdt_entry_t) * 4096 - 1;
    gdt_ptr.base = (uint32_t) &gdt_entries;

    // 设置 GDT 描述符
    gdt_set_gate(0, 0, 0,          0);     // NULL 描述符
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x409A); // 内核代码段 (Ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x4092); // 内核数据段 (Ring 0)
    
    // 新增 Ring 1 描述符
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0x40BA); // 用户代码段 (Ring 1) - DPL=1
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0x40B2); // 用户数据段 (Ring 1) - DPL=1

    gdt_flush((uint32_t) &gdt_ptr);
    //printf_OK("GDT Success");
}

extern void *intr_table[48];

static void idt_set_gate(uint8_t num, uint32_t offset, uint16_t sel, uint8_t flags)
{
    idt_entries[num].offset_low = offset & 0xFFFF;
    idt_entries[num].selector = sel;
    idt_entries[num].dw_count = 0;
    idt_entries[num].access_right = flags;
    idt_entries[num].offset_high = (offset >> 16) & 0xFFFF;
}

static void init_idt()
{   
    //printf_info("start IDT");
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base = (uint32_t) &idt_entries;

    // 初始化 PIC
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);

    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

    for (int i = 0; i < 32 + 16; i++) {
        idt_set_gate(i, (uint32_t) intr_table[i], 0x08, 0x8E);
    }

    // 修改系统调用中断门，设置 DPL=1 (0xEE)
    idt_set_gate(0x80, (uint32_t) syscall_handler, 0x08, 0xEE);

    idt_flush((uint32_t) &idt_ptr);
    //printf_OK("IDT Success");
}

void init_gdtidt()
{
    init_gdt();
    init_idt();
}