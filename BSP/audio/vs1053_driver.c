#include "vs1053_driver.h"
#include "fatfs.h"
#include <string.h>

/* 全局变量定义 */
VS1053_Config_t g_vs1053_config;
VS1053_PlayInfo_t g_vs1053_play_info;

/* 私有变量 */
static FIL audio_file;
static bool file_opened = false;

/* ============================================================================
 * 基础操作函数
 * ============================================================================ */

/**
 * @brief       初始化VS1053
 * @param       无
 * @retval      true: 成功, false: 失败
 */
bool vs1053_init(void)
{
    uint8_t retry = 0;
    
    /* 初始化端口 */
    vs1053_port_init();
    
    /* 硬件复位 - 按照正点原子的方式 */
    VS_RST_L();
    HAL_Delay(20);
    VS_XDCS_H();  /* 取消数据传输 */
    VS_XCS_H();   /* 取消命令传输 */
    VS_RST_H();
    
    /* 等待DREQ变高 */
    while (!VS_DREQ_READ() && retry < 200) {
        retry++;
        HAL_Delay(1);
    }
    
    if (retry >= 200) {
        g_vs1053_play_info.state = VS1053_STATE_ERROR;
        return false;
    }
    
    HAL_Delay(20);
    
    /* 软件复位 - 按照正点原子的方式 */
    vs1053_soft_reset();
    
    /* 测试通信 */
    if (!vs1053_test()) {
        g_vs1053_play_info.state = VS1053_STATE_ERROR;
        return false;
    }
    
    /* 加载默认配置 */
    vs1053_load_default_config();
    vs1053_apply_config(&g_vs1053_config);
    
    /* 初始化播放信息 */
    memset(&g_vs1053_play_info, 0, sizeof(VS1053_PlayInfo_t));
    g_vs1053_play_info.state = VS1053_STATE_IDLE;
    
    return true;
}

/**
 * @brief       硬件复位VS1053
 * @param       无
 * @retval      true: 成功, false: 失败
 */
bool vs1053_reset(void)
{
    VS_RST_L();
    HAL_Delay(20);
    
    VS_XCS_H();
    VS_XDCS_H(); 
    VS_RST_H();
    HAL_Delay(20);
    
    return vs1053_test();
}

/**
 * @brief       软件复位VS1053
 * @param       无
 * @retval      无
 */
void vs1053_soft_reset(void)
{
    uint8_t retry = 0;
    
    while (!VS_DREQ_READ()) {}  /* 等待软件复位结束 */
    
    /* 启动传输 - 这一步很重要 */
    vs1053_port_speed_low();
    VS_XDCS_H();  /* 确保数据接口禁用 */
    VS_XCS_H();   /* 确保命令接口禁用 */
    vs1053_port_rw(0xFF);  /* 启动传输 */
    
    retry = 0;
    
    /* 软件复位，新模式 */
    while (vs1053_read_cmd(SPI_MODE) != 0x0800) {
        vs1053_write_cmd(SPI_MODE, 0x0804);  /* 软件复位，新模式 */
        HAL_Delay(2);  /* 等待至少1.35ms */
        
        if (retry++ > 100) {
            break;
        }
    }
    
    while (!VS_DREQ_READ()) {}  /* 等待软件复位结束 */
    
    retry = 0;
    
    /* 设置VS10XX的时钟，3倍频，1.5xADD */
    while (vs1053_read_cmd(SPI_CLOCKF) != 0x9800) {
        vs1053_write_cmd(SPI_CLOCKF, 0x9800);  /* 设置VS10XX的时钟，3倍频，1.5xADD */
        
        if (retry++ > 100) {
            break;
        }
    }
    
    HAL_Delay(20);
}

/**
 * @brief       测试VS1053通信 - 先简单测试再做RAM测试
 * @param       无
 * @retval      true: 通信正常, false: 通信异常
 */
bool vs1053_test(void)
{
    uint16_t temp;
    
    /* 先做简单的寄存器读取测试 */
    temp = vs1053_read_cmd(SPI_MODE);
    if (temp == 0xFFFF || temp == 0x0000) {
        /* SPI通信可能有问题 */
        return false;
    }
    
    /* 进行RAM测试 */
    vs1053_write_cmd(SPI_MODE, 0x0820);  /* 进入VS10XX的测试模式 */
    
    while (!VS_DREQ_READ()) {}  /* 等待DREQ为高 */
    
    vs1053_port_speed_low();  /* 低速 */
    VS_XDCS_L();  /* xDCS = 0，选择VS10XX的数据接口 */
    vs1053_port_rw(0x4d);
    vs1053_port_rw(0xea);
    vs1053_port_rw(0x6d);
    vs1053_port_rw(0x54);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    HAL_Delay(150);
    VS_XDCS_H();
    
    temp = vs1053_read_cmd(SPI_HDAT0);  /* VS1003如果得到的值为0x807F，则表明完好; VS1053为0X83FF */
    
    vs1053_port_speed_high();
    
    if (temp == 0x83FF || temp == 0x807F) {  /* VS1053或VS1003测试通过 */
        return true;
    }
    
    return false;
}

/* ============================================================================
 * 寄存器操作函数
 * ============================================================================ */

/**
 * @brief       写VS1053命令寄存器
 * @param       addr: 寄存器地址
 * @param       data: 16位数据
 * @retval      无
 */
void vs1053_write_cmd(uint8_t addr, uint16_t data)
{
    while (!VS_DREQ_READ()) {}  /* 等待空闲 */
    
    vs1053_port_speed_low();  /* 低速模式 */
    VS_XDCS_H();              /* 确保数据接口禁用 */
    VS_XCS_L();               /* 使能命令接口 */
    vs1053_port_rw(VS_WRITE_COMMAND);
    vs1053_port_rw(addr);
    vs1053_port_rw(data >> 8);
    vs1053_port_rw(data & 0xFF);
    VS_XCS_H();               /* 禁用命令接口 */
    vs1053_port_speed_high(); /* 恢复高速模式 */
}

/**
 * @brief       读VS1053命令寄存器
 * @param       addr: 寄存器地址
 * @retval      16位寄存器值
 */
uint16_t vs1053_read_cmd(uint8_t addr)
{
    uint16_t temp;
    
    while (!VS_DREQ_READ()) {}  /* 等待空闲 */
    
    vs1053_port_speed_low();  /* 低速模式 */
    VS_XDCS_H();              /* 确保数据接口禁用 */
    VS_XCS_L();               /* 使能命令接口 */
    vs1053_port_rw(VS_READ_COMMAND);
    vs1053_port_rw(addr);
    temp = vs1053_port_rw(0xFF) << 8;
    temp |= vs1053_port_rw(0xFF);
    VS_XCS_H();               /* 禁用命令接口 */
    vs1053_port_speed_high(); /* 恢复高速模式 */
    
    return temp;
}

/* ============================================================================
 * 音频控制函数
 * ============================================================================ */

/**
 * @brief       设置音量
 * @param       volume: 音量值 (0-254, 越小声音越大)
 * @retval      无
 */
void vs1053_set_volume(uint8_t volume)
{
    uint16_t vol_reg = (volume << 8) | volume;  /* 左右声道相同 */
    vs1053_write_cmd(SPI_VOL, vol_reg);
    g_vs1053_config.volume = volume;
}

/**
 * @brief       设置低音增强
 * @param       freq: 低音频率 (2-15, 对应20Hz-150Hz)
 * @param       amp: 低音增益 (0-15, 对应0-22.5dB)
 * @retval      无
 */
void vs1053_set_bass(uint8_t freq, uint8_t amp)
{
    uint16_t bass_reg = (amp << 4) | freq;
    vs1053_write_cmd(SPI_BASS, bass_reg);
    g_vs1053_config.bass_freq = freq;
    g_vs1053_config.bass_amp = amp;
}

/**
 * @brief       设置高音增强
 * @param       freq: 高音频率 (1-15, 对应1kHz-15kHz)
 * @param       amp: 高音增益 (0-15, 对应0-10.5dB)
 * @retval      无
 */
void vs1053_set_treble(uint8_t freq, uint8_t amp)
{
    uint16_t treble_reg = (amp << 12) | (freq << 8);
    uint16_t bass_reg = vs1053_read_cmd(SPI_BASS);
    vs1053_write_cmd(SPI_BASS, (bass_reg & 0x00FF) | treble_reg);
    g_vs1053_config.treble_freq = freq;
    g_vs1053_config.treble_amp = amp;
}

/**
 * @brief       扬声器控制
 * @param       enable: true-开启, false-关闭
 * @retval      无
 */
void vs1053_speaker_control(bool enable)
{
    /* 通过GPIO4控制功放 (如果硬件支持) */
    g_vs1053_config.speaker_enable = enable;
    /* TODO: 实现GPIO4控制逻辑 */
}

/* ============================================================================
 * 播放控制函数
 * ============================================================================ */

/**
 * @brief       开始播放
 * @param       无
 * @retval      true: 成功, false: 失败
 */
bool vs1053_play_start(void)
{
    if (g_vs1053_play_info.state == VS1053_STATE_ERROR) {
        return false;
    }
    
    g_vs1053_play_info.state = VS1053_STATE_PLAYING;
    return true;
}

/**
 * @brief       暂停播放
 * @param       无
 * @retval      无
 */
void vs1053_play_pause(void)
{
    if (g_vs1053_play_info.state == VS1053_STATE_PLAYING) {
        g_vs1053_play_info.state = VS1053_STATE_PAUSED;
    }
}

/**
 * @brief       恢复播放
 * @param       无
 * @retval      无
 */
void vs1053_play_resume(void)
{
    if (g_vs1053_play_info.state == VS1053_STATE_PAUSED) {
        g_vs1053_play_info.state = VS1053_STATE_PLAYING;
    }
}

/**
 * @brief       停止播放
 * @param       无
 * @retval      无
 */
void vs1053_play_stop(void)
{
    g_vs1053_play_info.state = VS1053_STATE_STOPPED;
    
    /* 关闭文件 */
    if (file_opened) {
        f_close(&audio_file);
        file_opened = false;
    }
    
    /* 软复位清除缓冲区 */
    vs1053_soft_reset();
    
    g_vs1053_play_info.state = VS1053_STATE_IDLE;
}

/**
 * @brief       检查是否正在播放
 * @param       无
 * @retval      true: 正在播放, false: 未播放
 */
bool vs1053_is_playing(void)
{
    return (g_vs1053_play_info.state == VS1053_STATE_PLAYING);
}

/* ============================================================================
 * 文件播放函数
 * ============================================================================ */

/**
 * @brief       播放音频文件
 * @param       filename: 文件名
 * @retval      true: 开始播放, false: 失败
 */
bool vs1053_play_file(const char* filename)
{
    FRESULT res;
    
    /* 停止当前播放 */
    vs1053_play_stop();
    
    /* 打开文件 */
    res = f_open(&audio_file, filename, FA_READ);
    if (res != FR_OK) {
        g_vs1053_play_info.state = VS1053_STATE_ERROR;
        return false;
    }
    
    file_opened = true;
    
    /* 开始播放 */
    return vs1053_play_start();
}

/**
 * @brief       播放音频缓冲区数据
 * @param       buffer: 音频数据缓冲区
 * @param       size: 数据大小
 * @retval      无
 */
void vs1053_play_buffer(const uint8_t* buffer, uint32_t size)
{
    uint32_t i;
    
    if (g_vs1053_play_info.state != VS1053_STATE_PLAYING) {
        return;
    }
    
    /* 分块发送数据 */
    for (i = 0; i < size; i += 32) {
        uint16_t chunk_size = (size - i) > 32 ? 32 : (size - i);
        
        /* 等待VS1053准备好 */
        while (!VS_DREQ_READ()) {
            HAL_Delay(1);
        }
        
        /* 发送数据 */
        vs1053_write_data(&buffer[i], chunk_size);
    }
}

/**
 * @brief       获取解码时间
 * @param       无
 * @retval      解码时间(秒)
 */
uint32_t vs1053_get_decode_time(void)
{
    uint16_t decode_time = vs1053_read_cmd(SPI_DECODE_TIME);
    return decode_time;
}

/* ============================================================================
 * 状态查询函数
 * ============================================================================ */

/**
 * @brief       获取播放状态
 * @param       无
 * @retval      播放状态
 */
VS1053_State_t vs1053_get_state(void)
{
    return g_vs1053_play_info.state;
}

/**
 * @brief       获取播放信息
 * @param       info: 播放信息结构体指针
 * @retval      无
 */
void vs1053_get_play_info(VS1053_PlayInfo_t* info)
{
    if (info != NULL) {
        memcpy(info, &g_vs1053_play_info, sizeof(VS1053_PlayInfo_t));
        info->decode_time = vs1053_get_decode_time();
    }
}

/**
 * @brief       等待VS1053准备好
 * @param       timeout_ms: 超时时间(毫秒)
 * @retval      true: 准备好, false: 超时
 */
bool vs1053_wait_ready(uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();
    
    while (!VS_DREQ_READ()) {
        if ((HAL_GetTick() - start_time) > timeout_ms) {
            return false;
        }
        HAL_Delay(1);
    }
    return true;
}

/* ============================================================================
 * 配置管理函数
 * ============================================================================ */

/**
 * @brief       加载默认配置
 * @param       无
 * @retval      无
 */
void vs1053_load_default_config(void)
{
    g_vs1053_config.volume = 50;          /* 中等音量 */
    g_vs1053_config.bass_freq = 6;        /* 60Hz */
    g_vs1053_config.bass_amp = 8;         /* 12dB */
    g_vs1053_config.treble_freq = 10;     /* 10kHz */
    g_vs1053_config.treble_amp = 5;       /* 3.5dB */
    g_vs1053_config.speaker_enable = true;
}

/**
 * @brief       应用配置
 * @param       config: 配置结构体指针
 * @retval      无
 */
void vs1053_apply_config(const VS1053_Config_t* config)
{
    if (config != NULL) {
        vs1053_set_volume(config->volume);
        vs1053_set_bass(config->bass_freq, config->bass_amp);
        vs1053_set_treble(config->treble_freq, config->treble_amp);
        vs1053_speaker_control(config->speaker_enable);
    }
} 