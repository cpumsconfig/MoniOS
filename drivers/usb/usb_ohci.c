#include "usb.h"
#include <string.h>
#include <stdint.h>

#define OHCI_BASE 0xFEC00000
#define MAX_TD 32
#define MAX_QH 8

typedef struct {
    uint32_t cbp;
    uint32_t be;
    uint32_t next_td;
    uint32_t control;
} OHCI_TD;

typedef struct {
    uint32_t head_td;
    uint32_t element;
    uint32_t next_qh;
    uint32_t control;
} OHCI_QH;

typedef struct {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t  bmCBWFlags;
    uint8_t  bCBWLUN;
    uint8_t  bCBWCBLength;
    uint8_t  CBWCB[16];
} CBW;

typedef struct {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t  bCSWStatus;
} CSW;

// 静态 DMA 区域
static OHCI_TD td_pool[MAX_TD] __attribute__((aligned(16)));
static OHCI_QH qh_pool[MAX_QH] __attribute__((aligned(16)));
static CBW cbw __attribute__((aligned(16)));
static CSW csw __attribute__((aligned(16)));
static uint8_t dma_buf[512] __attribute__((aligned(16)));

// MMIO 指针（初始化函数里赋值）
static volatile uint32_t* ohci = 0;

static void delay() {
    for(volatile uint32_t i=0;i<500000;i++) asm volatile("nop");
}

static inline void mmio_write32(uint32_t reg, uint32_t val) {
    ohci[reg/4] = val;
}

static inline uint32_t mmio_read32(uint32_t reg) {
    return ohci[reg/4];
}

// 初始化 OHCI（调用时执行）
void usb_ohci_init(void) {
    ohci = (volatile uint32_t*)OHCI_BASE;

    memset(td_pool, 0, sizeof(td_pool));
    memset(qh_pool, 0, sizeof(qh_pool));
    memset(&cbw, 0, sizeof(cbw));
    memset(&csw, 0, sizeof(csw));
    memset(dma_buf, 0, sizeof(dma_buf));

    mmio_write32(0x00, 0x1);        // 控制器复位
    while(mmio_read32(0x00) & 0x1); // 等待复位
    mmio_write32(0x10, 0);          // 初始化控制段
    mmio_write32(0x00, 0x2);        // 启动
}

// 简单接口示例（DMA 地址必须是物理地址）
int usb_control_transfer(uint8_t dev, uint8_t* setup, uint32_t setup_phys,
                         uint8_t* data, uint32_t data_phys,
                         uint32_t len, int dir) {
    (void)dev; (void)setup; (void)setup_phys;
    (void)data; (void)data_phys; (void)len; (void)dir;
    // 占位实现，实际可添加 TD/QH 提交
    return 0;
}

int usb_ohci_bulk_transfer(uint8_t dev, uint8_t* cmd, uint32_t cmd_phys,
                           uint8_t cmd_len, uint8_t* buf, uint32_t buf_phys,
                           uint32_t len, int dir) {
    (void)dev; (void)cmd; (void)cmd_phys;
    (void)cmd_len; (void)buf; (void)buf_phys; (void)len; (void)dir;
    // 占位实现
    return 0;
}
