#include "audio_player.h"
#include "nt35310_alientek.h"
#include <string.h>

/* 全局变量定义 */
AudioPlayer_t g_audio_player;

/* 私有变量 */
static FIL audio_file;
static uint8_t audio_buffer[512];  /* 音频数据缓冲区 */
static bool file_opened = false;

/* ============================================================================
 * 初始化和配置函数
 * ============================================================================ */

/**
 * @brief       初始化音频播放器
 * @param       无
 * @retval      true: 成功, false: 失败
 */
bool audio_player_init(void)
{
    /* 清零播放器状态 */
    memset(&g_audio_player, 0, sizeof(AudioPlayer_t));
    
    /* 检查VS1053是否已初始化 */
    if (vs1053_get_state() == VS1053_STATE_ERROR) {
        return false;
    }
    
    /* 设置默认参数 */
    g_audio_player.volume = 70;            /* 默认音量70% */
    g_audio_player.play_mode = PLAY_MODE_SINGLE;
    g_audio_player.initialized = true;
    
    /* 应用默认音量 */
    audio_player_set_volume(g_audio_player.volume);
    
    return true;
}

/**
 * @brief       反初始化音频播放器
 * @param       无
 * @retval      无
 */
void audio_player_deinit(void)
{
    audio_player_stop();
    g_audio_player.initialized = false;
}

/**
 * @brief       检查播放器是否准备就绪
 * @param       无
 * @retval      true: 准备就绪, false: 未准备好
 */
bool audio_player_is_ready(void)
{
    return g_audio_player.initialized && (vs1053_get_state() != VS1053_STATE_ERROR);
}

/* ============================================================================
 * 播放控制函数
 * ============================================================================ */

/**
 * @brief       播放指定文件
 * @param       filename: 文件名
 * @retval      true: 开始播放, false: 失败
 */
bool audio_player_play_file(const char* filename)
{
    FRESULT res;
    
    if (!audio_player_is_ready()) {
        return false;
    }
    
    /* 停止当前播放 */
    audio_player_stop();
    
    /* 打开文件 */
    res = f_open(&audio_file, filename, FA_READ);
    if (res != FR_OK) {
        lcd_show_string(10, 200, 300, 16, 12, "File open failed!", RED);
        return false;
    }
    
    file_opened = true;
    
    /* 更新播放器状态 */
    strncpy(g_audio_player.current_file, filename, sizeof(g_audio_player.current_file) - 1);
    g_audio_player.current_file[sizeof(g_audio_player.current_file) - 1] = '\0';
    g_audio_player.playing = true;
    g_audio_player.paused = false;
    g_audio_player.play_time = 0;
    
    /* 开始播放 */
    if (!vs1053_play_start()) {
        f_close(&audio_file);
        file_opened = false;
        g_audio_player.playing = false;
        return false;
    }
    
    /* 显示播放信息 */
    char info_str[100];
    sprintf(info_str, "Playing: %.30s", filename);
    lcd_show_string(10, 180, 300, 16, 12, info_str, GREEN);
    
    return true;
}

/**
 * @brief       播放当前文件
 * @param       无
 * @retval      true: 成功, false: 失败
 */
bool audio_player_play_current(void)
{
    if (strlen(g_audio_player.current_file) > 0) {
        return audio_player_play_file(g_audio_player.current_file);
    }
    return false;
}

/**
 * @brief       暂停播放
 * @param       无
 * @retval      无
 */
void audio_player_pause(void)
{
    if (g_audio_player.playing && !g_audio_player.paused) {
        vs1053_play_pause();
        g_audio_player.paused = true;
        lcd_show_string(10, 200, 300, 16, 12, "Paused", YELLOW);
    }
}

/**
 * @brief       恢复播放
 * @param       无
 * @retval      无
 */
void audio_player_resume(void)
{
    if (g_audio_player.playing && g_audio_player.paused) {
        vs1053_play_resume();
        g_audio_player.paused = false;
        lcd_show_string(10, 200, 300, 16, 12, "Playing", GREEN);
    }
}

/**
 * @brief       停止播放
 * @param       无
 * @retval      无
 */
void audio_player_stop(void)
{
    if (g_audio_player.playing) {
        vs1053_play_stop();
        
        if (file_opened) {
            f_close(&audio_file);
            file_opened = false;
        }
        
        g_audio_player.playing = false;
        g_audio_player.paused = false;
        g_audio_player.play_time = 0;
        
        lcd_show_string(10, 200, 300, 16, 12, "Stopped", RED);
    }
}

/* ============================================================================
 * 音量控制函数
 * ============================================================================ */

/**
 * @brief       设置音量
 * @param       volume: 音量 (0-100)
 * @retval      无
 */
void audio_player_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    
    g_audio_player.volume = volume;
    
    /* 转换为VS1053音量值 (0-254, 越小声音越大) */
    uint8_t vs_volume = 254 - (volume * 254 / 100);
    vs1053_set_volume(vs_volume);
    
    /* 显示音量信息 */
    char vol_str[50];
    sprintf(vol_str, "Volume: %d%%", volume);
    lcd_show_string(10, 220, 300, 16, 12, vol_str, BLUE);
}

/**
 * @brief       获取音量
 * @param       无
 * @retval      音量值 (0-100)
 */
uint8_t audio_player_get_volume(void)
{
    return g_audio_player.volume;
}

/**
 * @brief       音量增加
 * @param       无
 * @retval      无
 */
void audio_player_volume_up(void)
{
    uint8_t new_volume = g_audio_player.volume + 10;
    if (new_volume > 100) new_volume = 100;
    audio_player_set_volume(new_volume);
}

/**
 * @brief       音量减少
 * @param       无
 * @retval      无
 */
void audio_player_volume_down(void)
{
    uint8_t new_volume = (g_audio_player.volume >= 10) ? (g_audio_player.volume - 10) : 0;
    audio_player_set_volume(new_volume);
}

/* ============================================================================
 * 状态查询函数
 * ============================================================================ */

/**
 * @brief       检查是否正在播放
 * @param       无
 * @retval      true: 正在播放, false: 未播放
 */
bool audio_player_is_playing(void)
{
    return g_audio_player.playing && !g_audio_player.paused;
}

/**
 * @brief       检查是否暂停
 * @param       无
 * @retval      true: 暂停, false: 未暂停
 */
bool audio_player_is_paused(void)
{
    return g_audio_player.paused;
}

/**
 * @brief       获取播放时间
 * @param       无
 * @retval      播放时间(秒)
 */
uint32_t audio_player_get_play_time(void)
{
    if (g_audio_player.playing) {
        return vs1053_get_decode_time();
    }
    return 0;
}

/**
 * @brief       获取当前播放文件名
 * @param       无
 * @retval      文件名指针
 */
const char* audio_player_get_current_file(void)
{
    return g_audio_player.current_file;
}

/* ============================================================================
 * 播放任务函数
 * ============================================================================ */

/**
 * @brief       音频播放任务 (需要在主循环中调用)
 * @param       无
 * @retval      无
 */
void audio_player_task(void)
{
    UINT bytes_read;
    FRESULT res;
    
    if (!g_audio_player.playing || g_audio_player.paused || !file_opened) {
        return;
    }
    
    /* 检查VS1053是否准备好接收数据 */
    if (!VS_DREQ_READ()) {
        return;
    }
    
    /* 读取音频数据 */
    res = f_read(&audio_file, audio_buffer, sizeof(audio_buffer), &bytes_read);
    if (res != FR_OK || bytes_read == 0) {
        /* 文件读取完毕或出错，停止播放 */
        audio_player_stop();
        lcd_show_string(10, 240, 300, 16, 12, "Playback finished", BLUE);
        return;
    }
    
    /* 发送数据到VS1053 */
    vs1053_play_buffer(audio_buffer, bytes_read);
    
    /* 更新播放时间显示 */
    static uint32_t last_time_update = 0;
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_time_update > 1000) {  /* 每秒更新一次 */
        uint32_t play_time = audio_player_get_play_time();
        char time_str[50];
        sprintf(time_str, "Time: %02d:%02d", play_time / 60, play_time % 60);
        lcd_show_string(10, 260, 300, 16, 12, time_str, CYAN);
        last_time_update = current_tick;
    }
}

/**
 * @brief       播放下一首歌曲
 * @param       无
 * @retval      true: 成功切换, false: 失败或已是最后一首
 */
bool audio_player_next(void)
{
    if (!audio_player_is_ready()) {
        return false;
    }
    
    /* 获取音频文件列表 */
    if (fs_get_audio_files(&g_file_list, "/") != FS_STATUS_OK) {
        return false;
    }
    
    if (g_file_list.count == 0) {
        return false;
    }
    
    /* 找到当前文件索引 */
    uint16_t current_idx = g_audio_player.current_index;
    
    /* 切换到下一首 */
    switch (g_audio_player.play_mode) {
        case PLAY_MODE_SINGLE:
        case PLAY_MODE_REPEAT_ALL:
            current_idx++;
            if (current_idx >= g_file_list.count) {
                if (g_audio_player.play_mode == PLAY_MODE_REPEAT_ALL) {
                    current_idx = 0;  /* 循环到第一首 */
                } else {
                    return false;     /* 单曲模式，已是最后一首 */
                }
            }
            break;
            
        case PLAY_MODE_REPEAT_ONE:
            /* 单曲循环，不切换 */
            break;
            
        case PLAY_MODE_RANDOM:
            /* 随机播放 */
            if (g_file_list.count > 1) {
                do {
                    current_idx = HAL_GetTick() % g_file_list.count;
                } while (current_idx == g_audio_player.current_index);
            }
            break;
    }
    
    /* 播放新文件 */
    g_audio_player.current_index = current_idx;
    char full_path[128];
    sprintf(full_path, "/%s", g_file_list.files[current_idx].name);
    
    return audio_player_play_file(full_path);
}

/**
 * @brief       播放上一首歌曲
 * @param       无
 * @retval      true: 成功切换, false: 失败或已是第一首
 */
bool audio_player_prev(void)
{
    if (!audio_player_is_ready()) {
        return false;
    }
    
    /* 获取音频文件列表 */
    if (fs_get_audio_files(&g_file_list, "/") != FS_STATUS_OK) {
        return false;
    }
    
    if (g_file_list.count == 0) {
        return false;
    }
    
    /* 找到当前文件索引 */
    uint16_t current_idx = g_audio_player.current_index;
    
    /* 切换到上一首 */
    switch (g_audio_player.play_mode) {
        case PLAY_MODE_SINGLE:
        case PLAY_MODE_REPEAT_ALL:
            if (current_idx == 0) {
                if (g_audio_player.play_mode == PLAY_MODE_REPEAT_ALL) {
                    current_idx = g_file_list.count - 1;  /* 循环到最后一首 */
                } else {
                    return false;                          /* 单曲模式，已是第一首 */
                }
            } else {
                current_idx--;
            }
            break;
            
        case PLAY_MODE_REPEAT_ONE:
            /* 单曲循环，不切换 */
            break;
            
        case PLAY_MODE_RANDOM:
            /* 随机播放 */
            if (g_file_list.count > 1) {
                do {
                    current_idx = HAL_GetTick() % g_file_list.count;
                } while (current_idx == g_audio_player.current_index);
            }
            break;
    }
    
    /* 播放新文件 */
    g_audio_player.current_index = current_idx;
    char full_path[128];
    sprintf(full_path, "/%s", g_file_list.files[current_idx].name);
    
    return audio_player_play_file(full_path);
}

/* ============================================================================
 * 测试函数
 * ============================================================================ */

/**
 * @brief       测试播放第一个找到的MP3文件
 * @param       无
 * @retval      true: 开始播放, false: 失败
 */
bool audio_player_test_play(void)
{
    if (!audio_player_is_ready()) {
        lcd_show_string(10, 160, 300, 16, 12, "Audio not ready!", RED);
        return false;
    }
    
    /* 扫描当前目录的音频文件 */
    if (fs_get_audio_files("/", &g_file_list) != FS_STATUS_OK) {
        lcd_show_string(10, 160, 300, 16, 12, "No audio files found!", RED);
        return false;
    }
    
    if (g_file_list.count == 0) {
        lcd_show_string(10, 160, 300, 16, 12, "No MP3 files found!", RED);
        return false;
    }
    
    /* 播放第一个音频文件 */
    char full_path[128];
    sprintf(full_path, "/%s", g_file_list.files[0].name);
    
    lcd_show_string(10, 160, 300, 16, 12, "Starting playback...", GREEN);
    
    return audio_player_play_file(full_path);
} 