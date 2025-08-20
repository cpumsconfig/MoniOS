#include "drivers/audio.h"
#include "drivers/dma.h"
#include "monios/common.h"

#define SB16_RESET      0x226
#define SB16_READ       0x22A
#define SB16_WRITE      0x22C
#define SB16_IRQ        5
#define SB16_DMA8_CHAN  1

static uint8_t* sb16_buffer = 0;
static uint32_t sb16_size = 0;
static uint32_t sb16_pos = 0;
static uint8_t sb16_playing = 0;
static uint8_t sb16_stereo = 0; // 0=mono, 1=stereo

static void sb16_delay(int count) {
    for (volatile int i=0;i<count*1000;i++)
        __asm__ volatile("nop");
}

// 检测 SB16
int sb16_hw_init() {
    outb(SB16_RESET,1);
    sb16_delay(100);
    outb(SB16_RESET,0);
    sb16_delay(100);
    return (inb(SB16_READ)==0xAA) ? 0 : -1;
}

// 内部函数：启动 DMA 播放
static void sb16_start_dma(uint8_t* buffer, uint16_t size) {
    dma_init_channel(SB16_DMA8_CHAN);
    dma_write(SB16_DMA8_CHAN,(uint32_t)buffer,size);

    if(sb16_stereo)
        outb(SB16_WRITE,0x16);  // stereo 8-bit PCM
    else
        outb(SB16_WRITE,0x14);  // mono 8-bit PCM

    outb(SB16_WRITE,(size-1)&0xFF);
    outb(SB16_WRITE,((size-1)>>8)&0xFF);
    sb16_playing = 1;
}

// 中断处理
void sb16_irq_handler() {
    if(!sb16_playing) return;
    sb16_pos += sb16_size;
    if(sb16_buffer[sb16_pos]!=0) {
        sb16_start_dma(sb16_buffer + sb16_pos, sb16_size);
    } else {
        sb16_playing = 0;
    }
    outb(SB16_WRITE,0xD0); // EOI
}

// 外部接口：播放音乐缓冲区
int sb16_hw_play_music(uint8_t* buffer,uint32_t size,uint32_t total,uint8_t stereo) {
    if(!buffer || size==0 || total==0) return -1;
    sb16_buffer = buffer;
    sb16_size = size;
    sb16_pos = 0;
    sb16_stereo = stereo;
    sb16_start_dma(sb16_buffer,sb16_size);
    return 0;
}
