// drivers/audio/sb16.h
#ifndef __DRIVERS_AUDIO_SB16_H__
#define __DRIVERS_AUDIO_SB16_H__

#include <stdint.h>
#include <stdbool.h>

/* SB16 基本寄存器定义 */
#define SB16_BASE          0x220
#define SB16_MIXER_ADDR    (SB16_BASE + 0x06)
#define SB16_MIXER_DATA    (SB16_BASE + 0x07)
#define SB16_DSP_RESET     (SB16_BASE + 0x06)
#define SB16_DSP_READ      (SB16_BASE + 0x0A)
#define SB16_DSP_WRITE     (SB16_BASE + 0x0C)
#define SB16_DSP_READSTAT  (SB16_BASE + 0x0E)

/* DSP 命令 */
#define DSP_CMD_SET_OUT_SR 0x41
#define DSP_CMD_8BIT_DAC   0x14

/* SB16 初始化和控制函数 */
bool sb16_reset(void);
void sb16_set_output_rate(uint32_t hz);
void sb16_mixer_write(uint8_t reg, uint8_t value);
bool sb16_dsp_wait_write_ready(void);
bool sb16_dsp_wait_read_ready(void);
void sb16_dsp_write(uint8_t val);
uint8_t sb16_dsp_read(void);

#endif /* __DRIVERS_AUDIO_SB16_H__ */
