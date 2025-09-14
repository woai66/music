/**
 ****************************************************************************************************
 * @file        audioplay.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-29
 * @brief       音频播放 应用代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20200429
 * 第一次发布
 *
 ****************************************************************************************************
 */

#ifndef __AUDIOPLAYER_H
#define __AUDIOPLAYER_H

#include "./SYSTEM/sys/sys.h"


void audio_vol_show(uint8_t vol);
void audio_msg_show(uint32_t lenth);
uint16_t audio_get_tnum(char *path);
void audio_index_show(uint16_t index, uint16_t total);

void audio_play(void);
uint8_t audio_play_song(char *pname);

#endif












