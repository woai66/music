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
    
    /* 跳过ID3标签 - 查找真正的MP3音频数据 */
    uint8_t header[10];
    UINT bytes_read_header;
    res = f_read(&audio_file, header, 10, &bytes_read_header);
    
    if (res == FR_OK && bytes_read_header == 10) {
        /* 检查是否为ID3v2标签 */
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
            /* ID3v2标签，计算标签大小并跳过 */
            uint32_t tag_size = ((uint32_t)header[6] << 21) | 
                               ((uint32_t)header[7] << 14) | 
                               ((uint32_t)header[8] << 7) | 
                               ((uint32_t)header[9]);
            
            /* 跳过ID3标签 */
            f_lseek(&audio_file, tag_size + 10);
            
            char debug_str[60];
            sprintf(debug_str, "Skipped ID3 tag: %lu bytes", tag_size + 10);
            lcd_show_string(10, 260, 300, 16, 12, debug_str, YELLOW);
        } else {
            /* 不是ID3标签，回到文件开头 */
            f_lseek(&audio_file, 0);
        }
    }
    
    /* 更新播放器状态 */
    strncpy(g_audio_player.current_file, filename, sizeof(g_audio_player.current_file) - 1);
    g_audio_player.current_file[sizeof(g_audio_player.current_file) - 1] = '\0';
    g_audio_player.playing = true;
    g_audio_player.paused = false;
    g_audio_player.play_time = 0;
    
    /* 设置音量 - VS1053音量寄存器：0x00最大，0xFE最小 */
    vs1053_set_volume(80);  /* 设置较大音量 (80/100) */
    
    /* 设置高速SPI模式 */
    vs1053_port_speed_high();
    
    /* 开始播放 */
    if (!vs1053_play_start()) {
        f_close(&audio_file);
        file_opened = false;
        g_audio_player.playing = false;
        lcd_show_string(10, 180, 300, 16, 12, "VS1053 start failed!", RED);
        return false;
    }
    
    /* 显示播放信息和VS1053状态 */
    char info_str[100];
    sprintf(info_str, "Playing: %.30s", filename);
    lcd_show_string(10, 180, 300, 16, 12, info_str, GREEN);
    
    /* 显示VS1053寄存器状态 */
    uint16_t mode_reg = vs1053_read_cmd(SPI_MODE);
    uint16_t status_reg = vs1053_read_cmd(SPI_STATUS);
    uint16_t vol_reg = vs1053_read_cmd(SPI_VOL);
    
    char reg_str[80];
    sprintf(reg_str, "MODE:0x%04X STATUS:0x%04X", mode_reg, status_reg);
    lcd_show_string(10, 200, 300, 16, 12, reg_str, MAGENTA);
    
    char vol_str[60];
    sprintf(vol_str, "VOL:0x%04X (L:%d R:%d)", vol_reg, (vol_reg >> 8) & 0xFF, vol_reg & 0xFF);
    lcd_show_string(10, 220, 300, 16, 12, vol_str, CYAN);
    
    /* 显示AUDATA寄存器（音频信息） */
    uint16_t audata_reg = vs1053_read_cmd(SPI_AUDATA);
    char audata_str[60];
    sprintf(audata_str, "AUDATA:0x%04X (SR:%dHz)", audata_reg, audata_reg & 0xFFFE);
    lcd_show_string(10, 240, 300, 16, 12, audata_str, YELLOW);
    
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
 * @brief       播放指定的音乐文件 - 参考正点原子的实现方式
 * @param       filename: 文件路径
 * @retval      0: 播放完成, 其他: 按键码或错误
 */
uint8_t audio_player_play_song(const char* filename)
{
    UINT bytes_read;
    FRESULT res;
    uint8_t rval = 0;
    uint16_t i = 0;
    
    if (!audio_player_is_ready()) {
        return 0xFF;
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
        return 0xFF;
    }
    
    file_opened = true;
    
    /* 跳过ID3标签 - 查找真正的MP3音频数据 */
    uint8_t header[10];
    UINT bytes_read_header;
    res = f_read(&audio_file, header, 10, &bytes_read_header);
    
    if (res == FR_OK && bytes_read_header == 10) {
        /* 检查是否为ID3v2标签 */
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
            /* ID3v2标签，计算标签大小并跳过 */
            uint32_t tag_size = ((uint32_t)header[6] << 21) | 
                               ((uint32_t)header[7] << 14) | 
                               ((uint32_t)header[8] << 7) | 
                               ((uint32_t)header[9]);
            
            /* 跳过ID3标签 */
            f_lseek(&audio_file, tag_size + 10);
            
            char debug_str[60];
            sprintf(debug_str, "Skipped ID3 tag: %lu bytes", tag_size + 10);
            lcd_show_string(10, 240, 300, 16, 12, debug_str, YELLOW);
        } else {
            /* 不是ID3标签，回到文件开头 */
            f_lseek(&audio_file, 0);
        }
    }
    
    /* 更新播放器状态 */
    strncpy(g_audio_player.current_file, filename, sizeof(g_audio_player.current_file) - 1);
    g_audio_player.current_file[sizeof(g_audio_player.current_file) - 1] = '\0';
    g_audio_player.playing = true;
    g_audio_player.paused = false;
    g_audio_player.play_time = 0;
    
    /* 使用正点原子的初始化序列 */
    vs1053_restart_play();          /* 重启播放 */
    vs1053_set_all();               /* 设置音量等信息 */
    vs1053_reset_decode_time();     /* 复位解码时间 */
    vs1053_port_speed_high();       /* 高速SPI */
    
    /* 重要：重新启用板载喇叭 (因为restart_play可能重置了设置) */
    vs1053_set_speaker(1);          /* 确保板载喇叭开启 */
    
    /* 开始播放 */
    if (!vs1053_play_start()) {
        f_close(&audio_file);
        file_opened = false;
        g_audio_player.playing = false;
        lcd_show_string(10, 260, 300, 16, 12, "VS1053 start failed!", RED);
        return 0xFF;
    }
    
    lcd_show_string(10, 260, 300, 16, 12, "Playing...", GREEN);
    
    /* 显示VS1053寄存器状态 - 调试信息 */
    uint16_t mode_reg = vs1053_read_cmd(SPI_MODE);
    uint16_t status_reg = vs1053_read_cmd(SPI_STATUS);
    uint16_t vol_reg = vs1053_read_cmd(SPI_VOL);
    char reg_str[80];
    sprintf(reg_str, "M:0x%04X S:0x%04X V:0x%04X", mode_reg, status_reg, vol_reg);
    lcd_show_string(10, 280, 300, 16, 12, reg_str, MAGENTA);
    
    /* 主播放循环 - 参考正点原子的方法 */
    static uint32_t loop_count = 0;
    while (rval == 0) {
        res = f_read(&audio_file, audio_buffer, sizeof(audio_buffer), &bytes_read);
        if (res != FR_OK || bytes_read == 0) {
            /* 播放完成 */
            char end_str[60];
            sprintf(end_str, "End: res=%d bytes=%d loops=%lu", (int)res, (int)bytes_read, loop_count);
            lcd_show_string(10, 360, 300, 16, 12, end_str, BLUE);
            rval = 1;  /* 表示播放完成 */
            break;
        }
        
                 loop_count++;
         
         /* 检查第一块数据 */
         if (loop_count == 1) {
             char first_data[80];
             sprintf(first_data, "1st: %02X %02X %02X %02X %02X %02X %02X %02X", 
                    audio_buffer[0], audio_buffer[1], audio_buffer[2], audio_buffer[3],
                    audio_buffer[4], audio_buffer[5], audio_buffer[6], audio_buffer[7]);
             lcd_show_string(10, 400, 300, 16, 12, first_data, GREEN);
             
             /* 检查是否为MP3同步帧 */
             if ((audio_buffer[0] == 0xFF) && ((audio_buffer[1] & 0xE0) == 0xE0)) {
                 lcd_show_string(10, 420, 300, 16, 12, "MP3 frame found!", GREEN);
             } else {
                 lcd_show_string(10, 420, 300, 16, 12, "No MP3 frame!", RED);
             }
         }
         
         /* 每10次循环显示一次文件读取状态 */
         if (loop_count % 10 == 0) {
             char file_str[60];
             sprintf(file_str, "Loop:%lu Read:%d bytes", loop_count, (int)bytes_read);
             lcd_show_string(10, 380, 300, 16, 12, file_str, YELLOW);
         }
        
        i = 0;
        /* 发送数据循环 - 添加详细调试 */
        static uint32_t total_sent = 0;
        static uint32_t dreq_busy_count = 0;
        
        do {
            /* 使用正点原子的方式发送数据 */
            if (vs1053_send_music_data(audio_buffer + i) == 0) {
                /* 发送成功，移动指针 */
                i += 32;
                total_sent += 32;
                
                /* 每发送1KB显示一次进度 */
                if (total_sent % 1024 == 0) {
                    char progress_str[60];
                    sprintf(progress_str, "Sent: %luKB DREQ_busy: %lu", total_sent/1024, dreq_busy_count);
                    lcd_show_string(10, 300, 300, 16, 12, progress_str, CYAN);
                }
            } else {
                /* VS1053忙碌 - 正点原子方式处理 */
                dreq_busy_count++;
                
                /* 这里可以处理按键，但我们先专注播放 */
                HAL_Delay(1);  /* 短暂延时 */
                
                /* 如果DREQ长时间忙碌，显示警告 */
                if (dreq_busy_count % 1000 == 0) {
                    char dreq_str[60];
                    sprintf(dreq_str, "DREQ busy: %lu", dreq_busy_count);
                    lcd_show_string(10, 320, 300, 16, 12, dreq_str, RED);
                }
            }
        } while (i < bytes_read);
        
        /* 显示播放时间和详细寄存器状态 */
        static uint32_t last_time_update = 0;
        static uint32_t last_decode_time = 0;
        uint32_t current_tick = HAL_GetTick();
        if (current_tick - last_time_update > 1000) {  /* 每1秒更新一次 */
            uint32_t play_time = vs1053_get_decode_time();
            char time_str[80];
            
            /* 检查解码时间是否在增加 */
            if (play_time != last_decode_time) {
                sprintf(time_str, "Time: %02d:%02d (DECODING OK!)", play_time / 60, play_time % 60);
                lcd_show_string(10, 480, 300, 16, 12, time_str, GREEN);
                last_decode_time = play_time;
            } else {
                sprintf(time_str, "Time: %02d:%02d (NOT DECODING!)", play_time / 60, play_time % 60);
                lcd_show_string(10, 480, 300, 16, 12, time_str, RED);
            }
            
            /* 显示详细的VS1053寄存器状态 */
            vs1053_debug_registers();
            
            last_time_update = current_tick;
        }
    }
    
    /* 播放结束，关闭文件 */
    f_close(&audio_file);
    file_opened = false;
    g_audio_player.playing = false;
    
    lcd_show_string(10, 300, 300, 16, 12, "Playback finished", BLUE);
    
    return rval;
}

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
            lcd_show_string(10, 360, 300, 16, 12, "Playback finished", BLUE);
            return;
        }
        
        buffer_index = 0;
        need_new_data = false;
        
        /* 调试：显示数据传输状态 */
        static uint32_t total_sent = 0;
        total_sent += bytes_read;
        char sent_str[60];
        sprintf(sent_str, "Sent: %lu bytes", total_sent);
        lcd_show_string(10, 340, 300, 16, 12, sent_str, MAGENTA);
    }
    
    /* 发送数据到VS1053 */
    if (buffer_index < bytes_read) {
        uint16_t send_size = (bytes_read - buffer_index >= 32) ? 32 : (bytes_read - buffer_index);
        vs1053_write_data(audio_buffer + buffer_index, send_size);
        buffer_index += send_size;
        
        /* 检查是否需要读取新数据 */
        if (buffer_index >= bytes_read) {
            need_new_data = true;
        }
    }
    
    /* 更新播放时间显示 */
    static uint32_t last_time_update = 0;
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_time_update > 1000) {
        uint32_t play_time = vs1053_get_decode_time();
        char time_str[50];
        sprintf(time_str, "Time: %02d:%02d", play_time / 60, play_time % 60);
        lcd_show_string(10, 280, 300, 16, 12, time_str, CYAN);
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
    if (!audio_player_is_ready()) {
        lcd_show_string(10, 320, 300, 16, 12, "Audio not ready!", RED);
        return false;
    }
    
    /* 扫描当前目录的音频文件 */
    FS_Status_t scan_result = fs_get_audio_files("0:/MUSIC", &g_file_list);
    if (scan_result != FS_STATUS_OK) {
        /* 尝试扫描根目录 */
        scan_result = fs_get_audio_files("0:/", &g_file_list);
        if (scan_result != FS_STATUS_OK) {
            lcd_show_string(10, 320, 300, 16, 12, "No audio files found!", RED);
            return false;
        }
    }
    
    if (g_file_list.count == 0) {
        lcd_show_string(10, 320, 300, 16, 12, "No audio files found!", RED);
        return false;
    }
    
    /* 确保当前索引有效 */
    if (g_audio_player.current_index >= g_file_list.count) {
        g_audio_player.current_index = 0;
    }
    
    /* 清理音乐显示区域 */
    lcd_fill(10, 320, 310, 400, WHITE);
    
    /* 显示当前播放信息 */
    char song_info[60];
    snprintf(song_info, sizeof(song_info), "Playing: %.40s", g_file_list.files[g_audio_player.current_index].name);
    lcd_show_string(10, 320, 300, 16, 12, song_info, BLACK);
    
    char status_info[30];
    snprintf(status_info, sizeof(status_info), "Song %d/%d", g_audio_player.current_index + 1, g_file_list.count);
    lcd_show_string(10, 340, 300, 16, 12, status_info, BLUE);
    
    /* 播放当前索引的文件 */
    uint8_t result = audio_player_play_song(g_file_list.files[g_audio_player.current_index].path);
    
    /* 根据返回值判断结果 */
    if (result == 0 || result == 1) {
        return true;   /* 播放成功或正常结束 */
    } else {
        lcd_show_string(10, 360, 300, 16, 12, "Playback failed!", RED);
        return false;  /* 播放失败 */
    }
} 