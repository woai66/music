/**
 ****************************************************************************************************
* @file        sdio_sdcard.h
 * @author      章永琪 - 移植适配正点原子源码
 * @version     V1.1
 * @date        2025-09-13
 * @brief       SD卡 驱动代码 - 兼容CubeMX生成方式
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板 战舰V3
 * 引脚配置:
 * PC8  -> SDIO_D0
 * PC9  -> SDIO_D1  
 * PC10 -> SDIO_D2
 * PC11 -> SDIO_D3
 * PC12 -> SDIO_CLK
 * PD2  -> SDIO_CMD
 *
 * 兼容性说明:
 * 1. 使用CubeMX生成的hsd句柄
 * 2. 兼容CubeMX的MX_SDIO_SD_Init()函数
 * 3. 提供优化的配置参数
 *
 ****************************************************************************************************
 */

#ifndef __SDIO_SDCARD_H
#define __SDIO_SDCARD_H

#include "main.h"
#include <stdio.h>
#include <string.h>

/******************************************************************************************/
/* SDIO传输时钟分频设置 */
#define SDIO_TRANSFER_CLK_DIV_MY           0x02    /* SDIO传输时钟频率 = SDIO_CLK / [CLKDIV + 2] */
                                                /* STM32F103 SDIO_CLK = 72MHz / 2 = 36MHz */
                                                /* 传输时钟 = 36MHz / (2+2) = 9MHz < 25MHz(最大值) */

#define SD_TIMEOUT             ((uint32_t)100000000)      /* 超时时间 */
#define SD_TRANSFER_OK         ((uint8_t)0x00)            /* 传输完成 */
#define SD_TRANSFER_BUSY       ((uint8_t)0x01)            /* SD卡忙 */ 

/* 根据 SD_HandleTypeDef 定义的宏，用于快速计算容量 */
#define SD_TOTAL_SIZE_BYTE(__Handle__)  (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>0)
#define SD_TOTAL_SIZE_KB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>10)
#define SD_TOTAL_SIZE_MB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>20)
#define SD_TOTAL_SIZE_GB(__Handle__)    (((uint64_t)((__Handle__)->SdCard.LogBlockNbr)*((__Handle__)->SdCard.LogBlockSize))>>30)

/******************************************************************************************/
/* 外部变量声明 */
extern HAL_SD_CardInfoTypeDef  g_sd_card_info_handle;      /* SD卡信息结构体 */

/* 函数声明 */
uint8_t sd_init(void);
uint8_t get_sd_card_info(HAL_SD_CardInfoTypeDef *cardinfo);
uint8_t get_sd_card_state(void);
uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);
uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);
void show_sdcard_info(void);
uint8_t sd_test_read(uint32_t secaddr, uint32_t seccnt);
void show_sd_debug_info(void);

/* SD卡测试管理函数 */
void sd_handle_key_test(void);              /* 处理按键测试 */
void sd_show_complete_info(void);           /* 显示完整SD卡信息 */
void sd_handle_filesystem_test(void);       /* 处理文件系统测试 (KEY2) */

#endif 