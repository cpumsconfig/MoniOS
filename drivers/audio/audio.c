// audio.c
#include "drivers/audio.h"

// beep（你已有的硬件实现）
extern void beep_hw_play(uint32_t frequency);
extern void beep_hw_stop();
extern void beep_hw_delay(int count);

// sb16（你已有）
extern int sb16_hw_init();
extern int sb16_hw_play_music(uint8_t* buffer, uint32_t size, uint32_t total, uint8_t stereo);
extern void sb16_irq_handler();

// ac97（本文新增）
extern int ac97_hw_init();
extern int ac97_hw_play_stereo16(uint32_t sample_rate, const int16_t* buffer, uint32_t frames);

void beep_play(uint32_t f){ beep_hw_play(f); }
void beep_stop(){ beep_hw_stop(); }
void beep_delay(int c){ beep_hw_delay(c); }

int sb16_play(uint8_t* buf, uint32_t size, uint32_t total, uint8_t stereo){
    return sb16_hw_play_music(buf, size, total, stereo);
}

int ac97_init(){ return ac97_hw_init(); }
int audio_play(uint8_t* buffer, uint32_t size) {
    return ac97_play_stereo16((uint16_t*)buffer, size / 2);
}
void audio_init(){
    sb16_hw_init();
    ac97_hw_init();
}