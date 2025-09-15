#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "main.h"
#include "vs1053_driver.h"
#include "filesystem.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 播放模式 */
typedef enum {
    PLAY_MODE_SINGLE = 0,       /* 单曲播放 */
    PLAY_MODE_REPEAT_ONE,       /* 单曲循环 */
    PLAY_MODE_REPEAT_ALL,       /* 全部循环 */
    PLAY_MODE_RANDOM            /* 随机播放 */
} PlayMode_t;

/* 播放器状态 */
typedef struct {
    bool initialized;           /* 是否已初始化 */
    bool playing;              /* 是否正在播放 */
    bool paused;               /* 是否暂停 */
    uint16_t current_index;    /* 当前播放文件索引 */
    uint32_t play_time;        /* 播放时间(秒) */
    PlayMode_t play_mode;      /* 播放模式 */
    char current_file[64];     /* 当前播放文件名 */
    uint8_t volume;            /* 音量 (0-100) */
} AudioPlayer_t;

/* 全局变量 */
extern AudioPlayer_t g_audio_player;

/* 函数声明 */

/* 初始化和配置 */
bool audio_player_init(void);
void audio_player_deinit(void);
bool audio_player_is_ready(void);

/* 播放控制 */
bool audio_player_play_file(const char* filename);
uint8_t audio_player_play_song(const char* filename);  /* 新增：参考正点原子的播放方法 */
bool audio_player_play_current(void);
void audio_player_pause(void);
void audio_player_resume(void);
void audio_player_stop(void);
bool audio_player_next(void);
bool audio_player_prev(void);

/* 音量控制 */
void audio_player_set_volume(uint8_t volume);
uint8_t audio_player_get_volume(void);
void audio_player_volume_up(void);
void audio_player_volume_down(void);

/* 播放模式 */
void audio_player_set_mode(PlayMode_t mode);
PlayMode_t audio_player_get_mode(void);

/* 状态查询 */
bool audio_player_is_playing(void);
bool audio_player_is_paused(void);
uint32_t audio_player_get_play_time(void);
const char* audio_player_get_current_file(void);
void audio_player_get_status(AudioPlayer_t* status);

/* 播放任务 */
void audio_player_task(void);           /* 主播放任务，需要在主循环中调用 */

/* 简单测试函数 */
bool audio_player_test_play(void);      /* 测试播放第一个找到的MP3文件 */

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H 