#include "vs1053_driver.h"
#include "fatfs.h"
#include "nt35310_alientek.h"
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
    
    /* 初始化时就启用板载喇叭 */
    vs1053_set_speaker(1);  /* 开启板载喇叭 */
    
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
    /* VS1053音量寄存器：0x00=最大音量，0xFE=最小音量 */
    /* 输入volume: 0-100，转换为VS1053格式 */
    uint8_t vs_vol = (100 - volume) * 254 / 100;  /* 反向映射 */
    if (vs_vol > 254) vs_vol = 254;  /* 限制最小音量 */
    
    uint16_t vol_reg = (vs_vol << 8) | vs_vol;  /* 左右声道相同 */
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
    
    /* 软复位VS1053 - 参考正点原子方法 */
    vs1053_soft_reset();
    
    /* 重置解码时间 */
    vs1053_write_cmd(SPI_DECODE_TIME, 0x0000);
    vs1053_write_cmd(SPI_DECODE_TIME, 0x0000);
    
    /* 设置播放模式 - 清除SM_TESTS位，确保正常播放模式 */
    uint16_t mode = vs1053_read_cmd(SPI_MODE);
    mode &= ~SM_TESTS;  /* 清除测试模式 */
    mode &= ~SM_RESET;  /* 清除复位位 */
    vs1053_write_cmd(SPI_MODE, mode);
    
    /* 等待准备就绪 */
    if (!vs1053_wait_ready(1000)) {
        return false;
    }
    
    /* 发送一些空数据来启动解码器 - 参考正点原子方法 */
    uint8_t init_data[32];
    memset(init_data, 0, sizeof(init_data));
    
    for (int i = 0; i < 4; i++) {  /* 发送4次32字节的空数据 */
        while (!VS_DREQ_READ()) {  /* 等待准备就绪 */
            HAL_Delay(1);
        }
        vs1053_write_data(init_data, 32);
    }
    
    g_vs1053_play_info.state = VS1053_STATE_PLAYING;
    return true;
}

/**
 * @brief       VS1053正弦波测试
 * @param       无
 * @retval      无
 */
void vs1053_test_sine_wave(void)
{
    uint16_t mode;
    
    /* 确保VS1053准备就绪 */
    while (!VS_DREQ_READ()) {
        HAL_Delay(1);
    }
    
    /* 设置VS1053为测试模式 */
    mode = vs1053_read_cmd(SPI_MODE);
    mode |= SM_TESTS;  /* 设置测试模式位 */
    vs1053_write_cmd(SPI_MODE, mode);
    
    /* 等待模式设置生效 */
    HAL_Delay(10);
    while (!VS_DREQ_READ()) {
        HAL_Delay(1);
    }
    
    /* 发送正弦波测试命令 (1KHz正弦波) */
    vs1053_port_speed_low();  /* 使用低速SPI进行测试 */
    
    VS_XDCS_L();  /* 选择数据接口 */
    vs1053_port_rw(0x53);  /* 正弦波测试命令 */
    vs1053_port_rw(0xEF);
    vs1053_port_rw(0x6E);
    vs1053_port_rw(0x44);
    vs1053_port_rw(0x00);  /* 1KHz频率 */
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    VS_XDCS_H();
    
    /* 播放正弦波3秒 */
    HAL_Delay(3000);
    
    /* 停止正弦波测试 */
    VS_XDCS_L();
    vs1053_port_rw(0x45);  /* 退出命令 */
    vs1053_port_rw(0x78);
    vs1053_port_rw(0x69);
    vs1053_port_rw(0x74);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    vs1053_port_rw(0x00);
    VS_XDCS_H();
    
    vs1053_port_speed_high();  /* 恢复高速SPI */
    
    /* 退出测试模式 */
    mode = vs1053_read_cmd(SPI_MODE);
    mode &= ~SM_TESTS;  /* 清除测试模式位 */
    vs1053_write_cmd(SPI_MODE, mode);
    
    /* 等待退出测试模式 */
    HAL_Delay(10);
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

/* ============================================================================
 * 正点原子兼容函数
 * ============================================================================ */

/**
 * @brief       重启播放 (参考正点原子实现)
 * @param       无
 * @retval      无
 */
void vs1053_restart_play(void)
{
    uint16_t temp;
    uint16_t i;
    uint8_t n;
    uint8_t vsbuf[32];
    
    /* 清零缓冲区 */
    for (n = 0; n < 32; n++) {
        vsbuf[n] = 0;
    }
    
    temp = vs1053_read_cmd(SPI_MODE);   /* 读取SPI_MODE的内容 */
    temp |= 1 << 3;                     /* 设置SM_CANCEL位 */
    temp |= 1 << 2;                     /* 设置SM_LAYER12位,允许播放MP1,MP2 */
    vs1053_write_cmd(SPI_MODE, temp);   /* 设置取消当前解码指令 */
    
    /* 发送2048个0,期间读取SM_CANCEL位.如果为0,则表示已经取消了当前解码 */
    for (i = 0; i < 2048;) {
        if (vs1053_send_music_data(vsbuf) == 0) {   /* 每发送32个字节后检测一次 */
            i += 32;                                /* 发送了32个字节 */
            temp = vs1053_read_cmd(SPI_MODE);       /* 读取SPI_MODE的内容 */
            
            if ((temp & (1 << 3)) == 0) {
                break;  /* 成功取消了 */
            }
        }
    }
    
    if (i < 2048) { /* SM_CANCEL正常 */
        /* 这里应该读取填充字节，但我们简化为直接使用0 */
        temp = 0;   /* 简化：直接使用0作为填充字节 */
        
        for (n = 0; n < 32; n++) {
            vsbuf[n] = temp;    /* 填充字节放入数组 */
        }
        
        for (i = 0; i < 2052;) {
            if (vs1053_send_music_data(vsbuf) == 0) {
                i += 32;    /* 填充 */
            }
        }
    } else {
        vs1053_soft_reset();    /* SM_CANCEL不成功,坏情况,需要软复位 */
    }
    
    temp = vs1053_read_cmd(SPI_HDAT0);
    temp += vs1053_read_cmd(SPI_HDAT1);
    
    if (temp) { /* 软复位,还是没有成功取消,放杀手锏,硬复位 */
        vs1053_reset();         /* 硬复位 */
        vs1053_soft_reset();    /* 软复位 */
    }
    
    /* 设置状态 */
    g_vs1053_play_info.state = VS1053_STATE_IDLE;
    
    /* 重新启用板载喇叭 (复位后需要重新设置) */
    vs1053_set_speaker(1);
}

/**
 * @brief       设置所有参数 (参考正点原子实现)
 * @param       无
 * @retval      无
 */
void vs1053_set_all(void)
{
    /* 完全按照正点原子的vs10xx_set_all实现 */
    
    /* 设置音量 - 正点原子使用vsset.mvol = 220 */
    uint16_t volt = 254 - 220;  /* 取反一下,得到最大值表示 */
    volt <<= 8;
    volt += 254 - 220;          /* 得到音量设置后大小 */
    vs1053_write_cmd(SPI_VOL, volt);  /* 设置音量 */
    
    /* 设置高低音(音调) - 正点原子的默认参数 */
    uint8_t bfreq = 6;      /* 低频上限频率 60Hz */
    uint8_t bass = 15;      /* 低频增益 15dB */
    uint8_t tfreq = 10;     /* 高频下限频率 10KHz */
    uint8_t treble = 15;    /* 高频增益 15dB */
    
    uint16_t bass_set = 0;
    signed char temp = (signed char)treble - 8;
    
    bass_set = temp & 0X0F;     /* 高音设定 */
    bass_set <<= 4;
    bass_set += tfreq & 0xf;    /* 高音下限频率 */
    bass_set <<= 4;
    bass_set += bass & 0xf;     /* 低音设定 */
    bass_set <<= 4;
    bass_set += bfreq & 0xf;    /* 低音上限 */
    vs1053_write_cmd(SPI_BASS, bass_set);   /* BASS */
    
    /* 设置空间效果 - 正点原子默认关闭 */
    uint16_t mode_reg = vs1053_read_cmd(SPI_MODE);
    mode_reg &= ~(1 << 4);  /* 取消 LO */
    mode_reg &= ~(1 << 7);  /* 取消 HI */
    vs1053_write_cmd(SPI_MODE, mode_reg);   /* 设定模式 */
    
    /* 设置板载喇叭 - 正点原子默认开启 */
    vs1053_set_speaker(1);  /* 开启板载喇叭 */
}

/**
 * @brief       重置解码时间 (参考正点原子实现)
 * @param       无
 * @retval      无
 */
void vs1053_reset_decode_time(void)
{
    /* 重置解码时间寄存器 */
    vs1053_write_cmd(SPI_DECODE_TIME, 0x0000);
    vs1053_write_cmd(SPI_DECODE_TIME, 0x0000);  /* 写两次确保重置 */
    
    /* 重置播放信息中的时间 */
    g_vs1053_play_info.decode_time = 0;
    
    /* 等待重置生效 */
    HAL_Delay(5);
}

/**
 * @brief       发送音乐数据 (安全的HAL库版本)
 * @param       buf: 数据缓冲区指针 (32字节)
 * @retval      0: 发送成功, 1: VS1053忙碌
 */
uint8_t vs1053_send_music_data(uint8_t* buf)
{
    uint8_t n;
    
    if (VS_DREQ_READ() != 0) {      /* 是否需要发送数据给VS1053? */
        VS_XDCS_L();                /* 选中数据传输 */
        
        for (n = 0; n < 32; n++) {  /* 发送32字节音频数据 */
            vs1053_port_rw(buf[n]);
        }
        
        VS_XDCS_H();                /* 取消数据传输 */
    }
    else {
        return 1;  /* VS1053忙碌，返回1 */
    }
    
    return 0;  /* 成功发送了 */
}

/**
 * @brief       简单的VS1053功能测试
 * @param       无
 * @retval      无
 */
void vs1053_simple_test(void)
{
    /* 这个函数专门用于调试VS1053基本功能 */
    
    /* 1. 测试SPI通信 */
    uint16_t mode = vs1053_read_cmd(SPI_MODE);
    if (mode == 0xFFFF || mode == 0x0000) {
        /* SPI通信失败 */
        return;
    }
    
    /* 2. 硬复位 + 软复位 */
    vs1053_reset();
    vs1053_soft_reset();
    
    /* 3. 设置音量 */
    vs1053_write_cmd(SPI_VOL, 0xDCDC);  /* 设置音量为220,220 */
    
    /* 4. 设置时钟频率 (这个很重要!) */
    vs1053_write_cmd(SPI_CLOCKF, 0x9800);  /* 3倍频 */
    
    /* 5. 清除所有模式位，确保正常播放模式 */
    vs1053_write_cmd(SPI_MODE, 0x0800);    /* 正常模式 */
    
    /* 6. 检查并设置音频输出相关寄存器 */
    vs1053_write_cmd(SPI_BASS, 0x0000);    /* 清除BASS设置 */
    
    /* 7. 重要：设置模拟输出参数 */
    /* 检查是否需要特殊的模拟输出设置 */
    
    /* 等待设置生效 */
    HAL_Delay(20);
    
    /* 8. 重置解码时间 */
    vs1053_write_cmd(SPI_DECODE_TIME, 0x0000);
    vs1053_write_cmd(SPI_DECODE_TIME, 0x0000);
}

/**
 * @brief       调试所有VS1053寄存器状态
 * @param       无
 * @retval      无
 */
void vs1053_debug_registers(void)
{
    uint16_t regs[16];
    
    /* 读取所有SCI寄存器 */
    regs[0] = vs1053_read_cmd(SPI_MODE);      /* 0x00 */
    regs[1] = vs1053_read_cmd(SPI_STATUS);    /* 0x01 */
    regs[2] = vs1053_read_cmd(SPI_BASS);      /* 0x02 */
    regs[3] = vs1053_read_cmd(SPI_CLOCKF);    /* 0x03 */
    regs[4] = vs1053_read_cmd(SPI_DECODE_TIME); /* 0x04 */
    regs[5] = vs1053_read_cmd(SPI_AUDATA);    /* 0x05 */
    regs[6] = vs1053_read_cmd(SPI_WRAM);      /* 0x06 */
    regs[7] = vs1053_read_cmd(SPI_WRAMADDR);  /* 0x07 */
    regs[8] = vs1053_read_cmd(SPI_HDAT0);     /* 0x08 */
    regs[9] = vs1053_read_cmd(SPI_HDAT1);     /* 0x09 */
    regs[10] = vs1053_read_cmd(SPI_AIADDR);   /* 0x0A */
    regs[11] = vs1053_read_cmd(SPI_VOL);      /* 0x0B */
    regs[12] = vs1053_read_cmd(SPI_AICTRL0);  /* 0x0C */
    regs[13] = vs1053_read_cmd(SPI_AICTRL1);  /* 0x0D */
    regs[14] = vs1053_read_cmd(SPI_AICTRL2);  /* 0x0E */
    regs[15] = vs1053_read_cmd(SPI_AICTRL3);  /* 0x0F */
    
    /* 显示关键寄存器 - AUDATA很重要，显示采样率和声道 */
    char audata_str[80];
    sprintf(audata_str, "AUDATA:0x%04X (SR:%dHz Ch:%d)", 
           regs[5], regs[5] & 0xFFFE, (regs[5] & 0x0001) ? 1 : 2);
    lcd_show_string(10, 440, 300, 16, 12, audata_str, YELLOW);
    
    /* 显示BASS寄存器 */
    char bass_str[60];
    sprintf(bass_str, "BASS:0x%04X HDAT0:0x%04X HDAT1:0x%04X", 
           regs[2], regs[8], regs[9]);
    lcd_show_string(10, 460, 300, 16, 12, bass_str, YELLOW);
}

/**
 * @brief       板载喇叭开/关设置函数 (完全按照正点原子实现)
 *   @note      战舰开发板 板载了HT6872功放, 通过VS1053的GPIO4(36脚), 控制其工作/关闭
 *              该函数实际上是VS1053的GPIO设置函数
 *              GPIO4 = 1, HT6872正常工作
 *              GPIO4 = 0, HT6872关闭(默认)
 * @param       sw      : 0, 关闭; 1, 开启;
 * @retval      无
 */
void vs1053_set_speaker(uint8_t sw)
{
    /* 需要实现vs1053_write_ram函数来写VS1053的RAM */
    /* GPIO_DDR = 0xC017, GPIO_ODATA = 0xC019 */
    
    /* VS1053的GPIO4设置成输出 */
    vs1053_write_cmd(SPI_WRAMADDR, 0xC017);  /* 设置RAM地址 */
    vs1053_write_cmd(SPI_WRAM, 1 << 4);      /* GPIO4设为输出 */
    
    /* 控制VS1053的GPIO4输出值(0/1) */
    vs1053_write_cmd(SPI_WRAMADDR, 0xC019);  /* 设置RAM地址 */
    vs1053_write_cmd(SPI_WRAM, sw << 4);     /* GPIO4输出值 */
} 