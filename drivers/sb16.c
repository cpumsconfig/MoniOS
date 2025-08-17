// sb16.c 修改如下
#include "sb16.h"
#include <stdbool.h>

#define BUFFER_SIZE 4096

static uint8_t buffer1[BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t buffer2[BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t* buffers[2] = { buffer1, buffer2 };
static volatile int active_buffer = 0;
static volatile bool playback_active = false;
static uint8_t* audio_data = NULL;
static uint32_t audio_size = 0;
static uint32_t audio_position = 0;

static void dsp_write(uint8_t cmd) {
    int timeout = 1000;
    while (timeout-- > 0 && (inb(DSP_WRITE_CMD) & 0x80));
    outb(DSP_WRITE_CMD, cmd);
}

static uint8_t dsp_read(void) {
    int timeout = 1000;
    while (timeout-- > 0 && !(inb(DSP_READ_STATUS) & 0x80));
    return inb(DSP_READ);
}

static void sb16_reset(void) {
    outb(DSP_RESET, 1);
    for (volatile int i = 0; i < 100; i++);
    outb(DSP_RESET, 0);
    for (volatile int i = 0; i < 100; i++);
    
    while (!(inb(DSP_READ_STATUS) & 0x80));
    if (dsp_read() != 0xAA) {
        // 初始化失败处理
    }
}

static void set_sample_rate(uint16_t rate) {
    dsp_write(0x41);
    dsp_write(rate >> 8);
    dsp_write(rate & 0xFF);
}

static void set_mixer_volume(uint8_t volume) {
    outb(MIXER_ADDR, 0x22);
    outb(MIXER_DATA, volume);
    outb(MIXER_ADDR, 0x24);
    outb(MIXER_DATA, volume);
    outb(MIXER_ADDR, 0x04);
    outb(MIXER_DATA, volume);
    outb(MIXER_ADDR, 0x05);
    outb(MIXER_DATA, volume);
}

static void fill_buffer(uint8_t* buffer, uint16_t size) {
    uint16_t bytes_to_copy = size;
    
    if (audio_data && audio_position < audio_size) {
        if (audio_position + size > audio_size) {
            bytes_to_copy = audio_size - audio_position;
        }
        
        for (uint16_t i = 0; i < bytes_to_copy; i++) {
            buffer[i] = audio_data[audio_position++];
        }
        
        for (uint16_t i = bytes_to_copy; i < size; i++) {
            buffer[i] = 0x80;
        }
    } else {
        for (uint16_t i = 0; i < size; i++) {
            buffer[i] = 0x80;
        }
    }
}

// 关键修改1：使用正确的单次模式命令(0x10)
static void start_playback(uint16_t buf_size) {
    dsp_write(0xC0);
    dsp_write(0x00);
    dsp_write(0x10);      // 单次模式命令
    dsp_write(buf_size & 0xFF);
    dsp_write(buf_size >> 8);
    dsp_write(0xD1);      // 正确打开扬声器
}

// 关键修改2：正确的关闭扬声器命令
void sb16_stop(void) {
    dsp_write(0xD0);      // 暂停DMA
    dsp_write(0xD3);      // 关闭扬声器
    playback_active = false;
    audio_data = NULL;
    audio_size = 0;
    audio_position = 0;
}

// 关键修改3：修复缓冲区切换逻辑
void sb16_irq_handler(registers_t* regs) {
    (void)regs;
    dsp_read();
    
    if (playback_active) {
        // 填充刚完成的缓冲区
        fill_buffer(buffers[active_buffer], BUFFER_SIZE);
        
        int next_buffer = 1 - active_buffer;
        
        // 关键修改4：使用单次DMA模式
        dma_setup(SB16_DMA_CHANNEL, (uint32_t)buffers[next_buffer], 
                 BUFFER_SIZE, DMA_MODE_READ | DMA_MODE_SINGLE);
        
        // 关键修改5：重新启动传输
        dsp_write(0x10);
        dsp_write(BUFFER_SIZE & 0xFF);
        dsp_write(BUFFER_SIZE >> 8);
        
        active_buffer = next_buffer;
        
        if (audio_position >= audio_size && audio_size > 0) {
            sb16_stop();
        }
    }
    
    outb(0x20, 0x20);
}

void sb16_init(void) {
    sb16_reset();
    set_sample_rate(22050);
    set_mixer_volume(0xFF);
    fill_buffer(buffer1, BUFFER_SIZE);
    fill_buffer(buffer2, BUFFER_SIZE);
    active_buffer = 0;
    
    // 使用单次DMA模式
    dma_setup(SB16_DMA_CHANNEL, (uint32_t)buffers[active_buffer], 
             BUFFER_SIZE, DMA_MODE_READ | DMA_MODE_SINGLE);
    
    register_interrupt_handler(IRQ5, sb16_irq_handler);
    playback_active = false;
}

void sb16_play(uint8_t* data, uint32_t size, uint16_t sample_rate) {
    if (playback_active) {
        sb16_stop();
    }
    
    audio_data = data;
    audio_size = size;
    audio_position = 0;
    
    if (sample_rate >= 5000 && sample_rate <= 48000) {
        set_sample_rate(sample_rate);
    }
    
    fill_buffer(buffer1, BUFFER_SIZE);
    fill_buffer(buffer2, BUFFER_SIZE);
    active_buffer = 0;
    
    dma_setup(SB16_DMA_CHANNEL, (uint32_t)buffers[active_buffer], 
             BUFFER_SIZE, DMA_MODE_READ | DMA_MODE_SINGLE);
    
    start_playback(BUFFER_SIZE);
    playback_active = true;
}