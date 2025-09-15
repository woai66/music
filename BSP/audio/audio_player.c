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
    
    /* 显示尝试打开的文件路径 */
    char path_debug[80];
    snprintf(path_debug, sizeof(path_debug), "Opening: %.50s", filename);
    lcd_show_string(10, 200, 300, 16, 12, path_debug, BLUE);
    
    /* 打开文件 */
    res = f_open(&audio_file, filename, FA_READ);
    if (res != FR_OK) {
        char error_msg[60];
        snprintf(error_msg, sizeof(error_msg), "File open failed! Error: %d", (int)res);
        lcd_show_string(10, 220, 300, 16, 12, error_msg, RED);
        
        /* 显示具体的FatFS错误信息 */
        const char* error_desc = "Unknown";
        switch(res) {
            case FR_NO_FILE: error_desc = "No file"; break;
            case FR_NO_PATH: error_desc = "No path"; break;
            case FR_INVALID_NAME: error_desc = "Invalid name"; break;
            case FR_DENIED: error_desc = "Access denied"; break;
            case FR_NOT_READY: error_desc = "Not ready"; break;
            case FR_WRITE_PROTECTED: error_desc = "Write protected"; break;
            case FR_DISK_ERR: error_desc = "Disk error"; break;
            case FR_INT_ERR: error_desc = "Internal error"; break;
            case FR_NOT_ENABLED: error_desc = "Not enabled"; break;
            case FR_NO_FILESYSTEM: error_desc = "No filesystem"; break;
        }
        
        char detailed_error[80];
        snprintf(detailed_error, sizeof(detailed_error), "FatFS: %.30s", error_desc);
        lcd_show_string(10, 240, 300, 16, 12, detailed_error, RED);
        
        return false;
    }
    
    file_opened = true;
    
    /* 更新播放器状态 */
    strncpy(g_audio_player.current_file, filename, sizeof(g_audio_player.current_file) - 1);
    g_audio_player.current_file[sizeof(g_audio_player.current_file) - 1] = '\0';
    g_audio_player.playing = true;
    g_audio_player.paused = false;
    g_audio_player.play_time = 0;
    
    /* 设置音量 - 参考正点原子的方法 */
    vs1053_set_volume(50);  /* 设置音量为50 (0-100) */
    
    /* 设置高速SPI模式 */
    vs1053_port_speed_high();
    
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
    static UINT bytes_read = 0;
    static uint16_t buffer_index = 0;
    static bool need_new_data = true;
    FRESULT res;
    
    if (!g_audio_player.playing || g_audio_player.paused || !file_opened) {
        return;
    }
    
    /* 检查VS1053是否准备好接收数据 */
    if (!VS_DREQ_READ()) {
        return;
    }
    
    /* 需要读取新数据 */
    if (need_new_data) {
        res = f_read(&audio_file, audio_buffer, sizeof(audio_buffer), &bytes_read);
        if (res != FR_OK || bytes_read == 0) {
            /* 文件读取完毕或出错，停止播放 */
            audio_player_stop();
            lcd_show_string(10, 240, 300, 16, 12, "Playback finished", BLUE);
            return;
        }
        buffer_index = 0;
        need_new_data = false;
    }
    
    /* 发送32字节数据到VS1053 - 参考正点原子的方法 */
    if (buffer_index < bytes_read) {
        uint16_t send_size = (bytes_read - buffer_index >= 32) ? 32 : (bytes_read - buffer_index);
        vs1053_write_data(audio_buffer + buffer_index, send_size);
        buffer_index += send_size;
        
        /* 调试信息：显示数据传输状态 */
        static uint32_t transfer_count = 0;
        transfer_count++;
        if (transfer_count % 100 == 0) {  /* 每传输100次显示一次 */
            char debug_str[60];
            sprintf(debug_str, "Sent: %lu KB", (transfer_count * 32) / 1024);
            lcd_show_string(10, 280, 300, 16, 12, debug_str, YELLOW);
        }
        
        /* 如果当前缓冲区数据发送完毕，标记需要新数据 */
        if (buffer_index >= bytes_read) {
            need_new_data = true;
        }
    }
    
    /* 更新播放时间显示 */
    static uint32_t last_time_update = 0;
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_time_update > 1000) {  /* 每秒更新一次 */
        uint32_t play_time = vs1053_get_decode_time();
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
    if (fs_get_audio_files("0:/MUSIC", &g_file_list) != FS_STATUS_OK) {
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
    
    return audio_player_play_file(g_file_list.files[current_idx].path);
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
    if (fs_get_audio_files("0:/MUSIC", &g_file_list) != FS_STATUS_OK) {
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
    
    return audio_player_play_file(g_file_list.files[current_idx].path);
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
    /* 清理之前的调试信息 */
    lcd_fill(10, 140, 310, 220, WHITE);
    
    if (!audio_player_is_ready()) {
        lcd_show_string(10, 160, 300, 16, 12, "Audio not ready!", RED);
        return false;
    }
    
    /* 显示文件系统状态 */
    char fs_status_str[60];
    sprintf(fs_status_str, "FS Mounted: %s", fs_is_mounted() ? "YES" : "NO");
    lcd_show_string(10, 120, 300, 16, 12, fs_status_str, fs_is_mounted() ? GREEN : RED);
    
    /* 扫描当前目录的音频文件 - 使用正点原子的路径格式 */
    FS_Status_t scan_result = fs_get_audio_files("0:/MUSIC", &g_file_list);
    if (scan_result != FS_STATUS_OK) {
        char debug_msg[60];
        sprintf(debug_msg, "Scan failed! Status: %d", (int)scan_result);
        lcd_show_string(10, 160, 300, 16, 12, debug_msg, RED);
        
        /* 尝试扫描根目录 */
        lcd_show_string(10, 180, 300, 16, 12, "Trying root directory...", YELLOW);
        scan_result = fs_get_audio_files("0:/", &g_file_list);
        if (scan_result != FS_STATUS_OK) {
            lcd_show_string(10, 200, 300, 16, 12, "Root scan also failed!", RED);
            return false;
        }
    }
    
    if (g_file_list.count == 0) {
        char debug_msg[60];
        sprintf(debug_msg, "Found 0 files in: %s", g_file_list.current_path);
        lcd_show_string(10, 160, 300, 16, 12, debug_msg, RED);
        return false;
    }
    
    /* 显示找到的文件数量 */
    char debug_msg[60];
    sprintf(debug_msg, "Found %d files in: %s", g_file_list.count, g_file_list.current_path);
    lcd_show_string(10, 140, 300, 16, 12, debug_msg, GREEN);
    
    /* 显示第一个文件的信息 */
    char file_info[80];
    snprintf(file_info, sizeof(file_info), "File[0]: %.40s", g_file_list.files[0].name);
    lcd_show_string(10, 160, 300, 16, 12, file_info, CYAN);
    
    char path_info[80];
    snprintf(path_info, sizeof(path_info), "Path: %.50s", g_file_list.files[0].path);
    lcd_show_string(10, 180, 300, 16, 12, path_info, CYAN);
    
    /* 播放第一个音频文件 */
    lcd_show_string(10, 200, 300, 16, 12, "Starting playback...", GREEN);
    
    return audio_player_play_file(g_file_list.files[0].path);
} 