#include "drivers/audio.h"
#include "monios/common.h"

#define PIT_CTRL 0x43
#define PIT_CHANNEL2 0x42
#define SPEAKER_CTRL 0x61

void beep_hw_play(uint32_t frequency) {
    uint16_t divisor = 1193180 / frequency;
    outb(PIT_CTRL,0xB6);
    outb(PIT_CHANNEL2,divisor&0xFF);
    outb(PIT_CHANNEL2,(divisor>>8)&0xFF);
    uint8_t tmp = inb(SPEAKER_CTRL);
    outb(SPEAKER_CTRL,tmp|3);
}

void beep_hw_stop() {
    uint8_t tmp = inb(SPEAKER_CTRL)&0xFC;
    outb(SPEAKER_CTRL,tmp);
}

void beep_hw_delay(int count) {
    for(volatile int i=0;i<count*1000;i++)
        __asm__ volatile("nop");
}
