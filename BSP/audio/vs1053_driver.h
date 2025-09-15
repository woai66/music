#ifndef VS1053_DRIVER_H
#define VS1053_DRIVER_H

#include "main.h"
#include "vs1053_port.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VS1053 寄存器定义 */
#define VS_WRITE_COMMAND    0x02
#define VS_READ_COMMAND     0x03

/* VS1053 SCI寄存器 */
#define SPI_MODE            0x00   /* 模式控制 */
#define SPI_STATUS          0x01   /* 状态 */
#define SPI_BASS            0x02   /* 低音增强 */
#define SPI_CLOCKF          0x03   /* 时钟频率 */
#define SPI_DECODE_TIME     0x04   /* 解码时间 */
#define SPI_AUDATA          0x05   /* 音频数据 */
#define SPI_WRAM            0x06   /* RAM写 */
#define SPI_WRAMADDR        0x07   /* RAM写地址 */
#define SPI_HDAT0           0x08   /* 流头数据0 */
#define SPI_HDAT1           0x09   /* 流头数据1 */
#define SPI_AIADDR          0x0A   /* 应用中断地址 */
#define SPI_VOL             0x0B   /* 音量控制 */
#define SPI_AICTRL0         0x0C   /* 应用中断控制0 */
#define SPI_AICTRL1         0x0D   /* 应用中断控制1 */
#define SPI_AICTRL2         0x0E   /* 应用中断控制2 */
#define SPI_AICTRL3         0x0F   /* 应用中断控制3 */

/* 模式寄存器位定义 */
#define SM_DIFF             0x0001  /* 差分输入 */
#define SM_JUMP             0x0002  /* 允许跳跃 */
#define SM_RESET            0x0004  /* 软件复位 */
#define SM_OUTOFWAV         0x0008  /* 跳出WAV解码 */
#define SM_PDOWN            0x0010  /* 功率下降模式 */
#define SM_TESTS            0x0020  /* 允许SDI测试 */
#define SM_STREAM           0x0040  /* 流模式 */
#define SM_PLUSV            0x0080  /* 允许PLUS-V */
#define SM_DACT             0x0100  /* DCLK激活边沿 */
#define SM_SDIORD           0x0200  /* SDI位顺序 */
#define SM_SDISHARE         0x0400  /* SDI共享 */
#define SM_SDINEW           0x0800  /* VS1002原生SPI模式 */
#define SM_ADPCM            0x1000  /* ADPCM录音激活 */
#define SM_ADPCM_HP         0x2000  /* ADPCM高通滤波器 */
#define SM_LINE1            0x4000  /* MIC/LINE1选择 */
#define SM_CLK_RANGE        0x8000  /* 输入时钟范围 */

/* 播放状态 */
typedef enum {
    VS1053_STATE_IDLE = 0,      /* 空闲 */
    VS1053_STATE_PLAYING,       /* 播放中 */
    VS1053_STATE_PAUSED,        /* 暂停 */
    VS1053_STATE_STOPPED,       /* 停止 */
    VS1053_STATE_ERROR          /* 错误 */
} VS1053_State_t;

/* VS1053 配置结构体 */
typedef struct {
    uint8_t volume;             /* 音量 (0-254) */
    uint8_t bass_freq;          /* 低音频率 */
    uint8_t bass_amp;           /* 低音增益 */
    uint8_t treble_freq;        /* 高音频率 */
    uint8_t treble_amp;         /* 高音增益 */
    bool speaker_enable;        /* 扬声器使能 */
} VS1053_Config_t;

/* 播放信息结构体 */
typedef struct {
    VS1053_State_t state;       /* 播放状态 */
    uint32_t decode_time;       /* 解码时间(秒) */
    uint16_t bitrate;           /* 比特率 */
    uint16_t sample_rate;       /* 采样率 */
    uint8_t channels;           /* 声道数 */
    bool is_mp3;                /* 是否为MP3格式 */
} VS1053_PlayInfo_t;

/* 全局变量声明 */
extern VS1053_Config_t g_vs1053_config;
extern VS1053_PlayInfo_t g_vs1053_play_info;

/* 函数声明 */

/* 基础操作 */
bool vs1053_init(void);
bool vs1053_reset(void);
bool vs1053_test(void);
void vs1053_soft_reset(void);
void vs1053_test_sine_wave(void);

/* 寄存器操作 */
void vs1053_write_cmd(uint8_t addr, uint16_t data);
uint16_t vs1053_read_cmd(uint8_t addr);
void vs1053_write_data(const uint8_t *buf, uint16_t len);

/* 音频控制 */
void vs1053_set_volume(uint8_t volume);
void vs1053_set_bass(uint8_t freq, uint8_t amp);
void vs1053_set_treble(uint8_t freq, uint8_t amp);
void vs1053_speaker_control(bool enable);

/* 播放控制 */
bool vs1053_play_start(void);
void vs1053_play_pause(void);
void vs1053_play_resume(void);
void vs1053_play_stop(void);
bool vs1053_is_playing(void);

/* 正点原子兼容函数 */
void vs1053_restart_play(void);      /* 重启播放 */
void vs1053_set_all(void);           /* 设置所有参数 */
void vs1053_reset_decode_time(void); /* 重置解码时间 */
uint8_t vs1053_send_music_data(uint8_t* buf); /* 发送音乐数据(32字节) */
void vs1053_simple_test(void);              /* 简单的VS1053功能测试 */
void vs1053_debug_registers(void);          /* 调试所有寄存器状态 */
void vs1053_set_speaker(uint8_t sw);        /* 设置板载喇叭开关 */

/* 文件播放 */
bool vs1053_play_file(const char* filename);
void vs1053_play_buffer(const uint8_t* buffer, uint32_t size);
uint32_t vs1053_get_decode_time(void);

/* 状态查询 */
VS1053_State_t vs1053_get_state(void);
void vs1053_get_play_info(VS1053_PlayInfo_t* info);
bool vs1053_wait_ready(uint32_t timeout_ms);

/* 配置管理 */
void vs1053_load_default_config(void);
void vs1053_apply_config(const VS1053_Config_t* config);

#ifdef __cplusplus
}
#endif

#endif // VS1053_DRIVER_H 