// ac97.c
#include <stdint.h>
#include "drivers/audio.h"
#include "monios/common.h"   // 需要 inb/outb/inw/outw/inl/outl（若无 inl/outl，可在此定义内联版）
#include <stddef.h> 
#include "math.h"
// ---- 若你的 common.h 只有 inb/outb，可解开下面两段内联 ----
// static inline void outl(uint16_t port, uint32_t val){ __asm__ volatile("outl %0,%1"::"a"(val),"Nd"(port)); }
// static inline uint32_t inl(uint16_t port){ uint32_t r; __asm__ volatile("inl %1,%0":"=a"(r):"Nd"(port)); return r; }

// ---------- PCI config access ----------
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline uint32_t pci_cfg_addr(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off){
    return (uint32_t)(0x80000000u | (bus<<16) | (dev<<11) | (fn<<8) | (off & 0xFC));
}
static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off){
    outl(PCI_CONFIG_ADDR, pci_cfg_addr(bus,dev,fn,off));
    return inl(PCI_CONFIG_DATA);
}
static uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off){
    uint32_t v = pci_read32(bus,dev,fn,off&0xFC);
    return (v >> ((off&2)*16)) & 0xFFFF;
}

// ---------- AC'97 PCI class codes ----------
#define PCI_CLASS_MULTIMEDIA   0x04
#define PCI_SUBCLASS_AUDIO     0x01

// ---------- AC'97 BARs (I/O space) ----------
static uint16_t ac97_nam_base = 0;   // Native Audio Mixer base (mixer regs)
static uint16_t ac97_nabm_base = 0;  // Native Audio Bus Master base (DMA regs)
static int ac97_found = 0;

// ---------- NAM (Mixer) 常用寄存器 ----------
#define NAM_RESET          0x00    // R/W, 写1触发软复位
#define NAM_MASTER_VOL     0x02    // 主音量 L/R（各5bit步进，0x0000最大，0x1F1F静音）
#define NAM_PCM_OUT_VOL    0x18    // PCM 输出音量
#define NAM_EXT_AUDIO_ID   0x28    // 读：扩展能力
#define NAM_FRONT_DAC_RATE 0x2C    // 前端 DAC 采样率 (RW)

// ---------- NABM (Bus Master) 寄存器（以 PCM OUT 通道为例） ----------
#define NABM_GLOB_CNT      0x2C
#define NABM_GLOB_STAT     0x30

// PCM OUT 通道寄存器偏移（相对 NABM base）
#define PO_BDBAR           0x10    // Buffer Descriptor Base Address
#define PO_CIV             0x14    // Current Index Value
#define PO_LVI             0x15    // Last Valid Index
#define PO_SR              0x16    // Status (RO/WC)
#define PO_PICB            0x18    // Position In Current Buffer (RO)
#define PO_CR              0x1B    // Control (RUN/RESET/IOCE/...)

#define PO_CR_RPBM         0x01    // RUN
#define PO_CR_RESET        0x02
#define PO_CR_IOCE         0x08    // 中断使能（可选）

#define PO_SR_DMA_HALTED   0x01
#define PO_SR_CELV         0x02
#define PO_SR_LVI          0x04
#define PO_SR_BCIS         0x08    // Buffer Completion Interrupt Status
#define PO_SR_FIFOE        0x10
#define PO_SR_MASK_ALL     0x1F

// ---------- BDL 结构 ----------
#pragma pack(push,1)
typedef struct {
    uint32_t buf_phys;  // 物理地址（对简化内核，假设等于线性地址）
    uint16_t buf_len;   // 长度（字节，偶数，最大 0xFFFF）
    uint8_t  reserved;
    uint8_t  flags;     // bit0=IOC（缓冲完成中断），bit1=BUP
} ac97_bdl_entry_t;
#pragma pack(pop)

// BDL 缓冲
#define AC97_BDL_ENTRIES 32
static ac97_bdl_entry_t ac97_bdl[AC97_BDL_ENTRIES];

// 简单延时
static inline void io_delay(int n){ for(volatile int i=0;i<n*1000;i++) __asm__ volatile("nop"); }

// ------------------ 探测并初始化 AC'97 ------------------
static int ac97_pci_probe(){
    for(uint8_t bus=0; bus<0xFF; ++bus){
        for(uint8_t dev=0; dev<32; ++dev){
            for(uint8_t fn=0; fn<8; ++fn){
                uint16_t vendor = pci_read16(bus,dev,fn,0x00);
                if(vendor==0xFFFF) continue;

                uint8_t class = (uint8_t)(pci_read32(bus,dev,fn,0x08) >> 24);
                uint8_t subclass = (uint8_t)(pci_read32(bus,dev,fn,0x08) >> 16);

                if(class==PCI_CLASS_MULTIMEDIA && subclass==PCI_SUBCLASS_AUDIO){
                    // 读 BAR0 (NAM), BAR1 (NABM)
                    uint32_t bar0 = pci_read32(bus,dev,fn,0x10);
                    uint32_t bar1 = pci_read32(bus,dev,fn,0x14);
                    if(!(bar0&1) || !(bar1&1)) continue; // 必须是 I/O BAR
                    ac97_nam_base  = (uint16_t)(bar0 & ~0x3);  // 对齐
                    ac97_nabm_base = (uint16_t)(bar1 & ~0x3);
                    return 0;
                }
            }
        }
    }
    return -1;
}

static void ac97_codec_reset(){
    // 软复位：写 NAM:0x00
    outw(ac97_nam_base + NAM_RESET, 0x0000);
    io_delay(100);

    // 可选：总线主控全局复位（某些 ICH 需要），这里做个轻微 reset
    uint32_t gc = inl(ac97_nabm_base + NABM_GLOB_CNT);
    outl(ac97_nabm_base + NABM_GLOB_CNT, gc | 0x02); // 冷复位位（不同 ICH 可能位不同，最小实现可略过）
    io_delay(100);
    outl(ac97_nabm_base + NABM_GLOB_CNT, gc & ~0x02);
    io_delay(100);
}

static void ac97_set_sample_rate(uint32_t rate){
    // 设置前端 DAC 采样率（常见 48000 或 44100）
    outw(ac97_nam_base + NAM_FRONT_DAC_RATE, (uint16_t)rate);
    io_delay(10);
    (void)inw(ac97_nam_base + NAM_FRONT_DAC_RATE); // 读回可选
}

static void ac97_unmute(){
    // 主音量/PCM 音量设为较大（AC'97 低值更响）
    outw(ac97_nam_base + NAM_MASTER_VOL, 0x0000); // Max
    outw(ac97_nam_base + NAM_PCM_OUT_VOL, 0x0000);
}

// --------- 对外：初始化 ----------
int ac97_hw_init(){
    if(ac97_pci_probe()!=0) return -1;
    ac97_codec_reset();
    ac97_unmute();
    ac97_set_sample_rate(48000);
    ac97_found = 1;
    return 0;
}

// --------- 内部：停止 & 复位播放通道 ----------
static void ac97_po_stop_reset(){
    // 停止
    uint8_t cr = inb(ac97_nabm_base + PO_CR);
    cr &= ~PO_CR_RPBM;
    outb(ac97_nabm_base + PO_CR, cr);

    // 等待 DMA 停止
    for(int i=0;i<1000;i++){
        if(inb(ac97_nabm_base + PO_SR) & PO_SR_DMA_HALTED) break;
        io_delay(1);
    }
    // 复位通道
    outb(ac97_nabm_base + PO_CR, PO_CR_RESET);
    io_delay(10);
    outb(ac97_nabm_base + PO_CR, 0x00);

    // 清状态
    outb(ac97_nabm_base + PO_SR, PO_SR_MASK_ALL);
}

// --------- 对外：播放 16-bit 立体声 PCM ----------
int ac97_hw_play_stereo16(uint32_t sample_rate, const int16_t* buffer, uint32_t frames){
    if(!ac97_found) return -1;
    if(!buffer || frames==0) return -2;

    // 设置采样率
    ac97_set_sample_rate(sample_rate);

    // 准备 BDL
    // 一帧=左右各16bit=4字节，按块拆分，每块 <= 0xFFF0 以对齐且避免超长
    const uint32_t bytes_total = frames * 4u;
    const uint8_t  blk_cnt_max = AC97_BDL_ENTRIES;
    const uint32_t blk_sz      = 0x1000; // 4KB/块（可按需调整）

    uint32_t remain = bytes_total;
    uint32_t addr   = (uint32_t)(uintptr_t)buffer; // 简化：物理=线性
    uint8_t ndesc   = 0;

    while(remain && ndesc < blk_cnt_max){
        uint32_t sz = (remain > blk_sz) ? blk_sz : remain;
        // AC'97 要求偶数长度
        if(sz & 1) sz--;

        ac97_bdl[ndesc].buf_phys = addr;
        ac97_bdl[ndesc].buf_len  = (uint16_t)(sz & 0xFFFF);
        ac97_bdl[ndesc].reserved = 0;
        ac97_bdl[ndesc].flags    = (ndesc == blk_cnt_max-1 || sz==remain) ? 0x01 /*IOC*/ : 0x00;

        addr   += sz;
        remain -= sz;
        ndesc++;
    }
    if(ndesc==0) return -3;

    // 停止/复位通道并清状态
    ac97_po_stop_reset();

    // 写入 BDL 基址
    outl(ac97_nabm_base + PO_BDBAR, (uint32_t)(uintptr_t)ac97_bdl);

    // CIV=0，LVI=ndesc-1
    outb(ac97_nabm_base + PO_CIV, 0x00);
    outb(ac97_nabm_base + PO_LVI, (uint8_t)(ndesc-1));

    // 清状态位
    outb(ac97_nabm_base + PO_SR, PO_SR_MASK_ALL);

    // 运行（可选打开 IOCE 产生中断）
    outb(ac97_nabm_base + PO_CR, PO_CR_RPBM /*| PO_CR_IOCE*/);

    return 0;
}

// 对外包装，给 audio.c 调用
int ac97_play_stereo16(uint32_t sample_rate, const int16_t* buffer, uint32_t frames){
    return ac97_hw_play_stereo16(sample_rate, buffer, frames);
}
