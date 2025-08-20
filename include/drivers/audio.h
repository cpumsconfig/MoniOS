// audio.h
#ifndef __AUDIO_H
#define __AUDIO_H

#include <stdint.h>

// ---- 蜂鸣器 ----
void beep_play(uint32_t frequency);
void beep_stop();
void beep_delay(int count);

// ---- SB16（你已有）----
int sb16_play(uint8_t* buffer, uint32_t size, uint32_t total, uint8_t stereo);
int sb16_hw_init();
void sb16_irq_handler();

// ---- AC'97 新增接口 ----
int ac97_init();  // 探测并初始化 AC'97
// 播放 16-bit 立体声 PCM：sample_rate(常用48000/44100)，buffer 指向交错LR的int16_t数组，frames=帧数（每帧2声道）
int audio_play(uint8_t* buffer, uint32_t size);

#endif
