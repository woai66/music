/**
 ****************************************************************************************************
 * @file        sdio_sdcard.c
 * @author      章永琪 - 移植适配正点原子源码
 * @version     V1.1
 * @date        2025-09-13
 * @brief       SD卡 驱动代码 - 使用STM32CubeMX生成工程
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板 战舰V3
 * 移植说明:
 * 1. 兼容CubeMX生成的代码，使用外部hsd句柄
 * 2. 修复32GB大容量SD卡兼容性
 * 3. 优化SDIO配置参数
 * 
 ****************************************************************************************************
 */

#include "sdio_sdcard.h"
#include "nt35310_alientek.h"
#include "sdio.h"  /* 包含CubeMX生成的SDIO配置 */

/* 使用CubeMX生成的外部句柄，而不是我们自己的 */
extern SD_HandleTypeDef hsd;

/* 我们的SD卡信息结构体 */
HAL_SD_CardInfoTypeDef g_sd_card_info_handle;

/**
 * @brief       初始化SD卡 - 兼容CubeMX方式，增强调试
 * @param       无
 * @retval      返回值:0 初始化正确；其他值，初始化错误
 */
uint8_t sd_init(void)
{
    HAL_StatusTypeDef result;
    
    /* 先使用CubeMX的初始化，但我们需要修正一些参数 */
    MX_SDIO_SD_Init();
    
    /* 重新配置关键参数以提高兼容性 */
    hsd.Init.ClockDiv = SDIO_TRANSFER_CLK_DIV;                         /* 修正时钟分频，避免过快 */
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_ENABLE;  /* 启用硬件流控 */
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;                               /* 先用1位模式，更稳定 */
    
    /* 重新初始化以应用新参数 */
    result = HAL_SD_Init(&hsd);
    if (result != HAL_OK)
    {
        return 1;   /* 初始化失败 */
    }
    
    /* 检查SD卡状态 */
    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd);
    if (cardState != HAL_SD_CARD_TRANSFER)
    {
        return 2;   /* SD卡状态异常 */
    }
    
    /* 尝试配置4位宽总线（渐进式，如果失败就继续使用1位模式） */
    result = HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B);
    if (result != HAL_OK)
    {
        /* 4位模式失败，保持1位模式，这对大多数SD卡仍然有效 */
        /* 不返回错误，继续使用1位模式 */
        
        /* 可选：再次确认1位模式工作正常 */
        result = HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B);
        if (result != HAL_OK)
        {
            return 3;   /* 连1位模式都失败 */
        }
    }
    
    return 0;   /* 初始化成功 */
}

/**
 * @brief       获取SD卡详细状态信息（调试用）
 * @param       无  
 * @retval      无
 */
void show_sd_debug_info(void)
{
    char debug_str[60];
    
    /* 显示SDIO配置信息 */
    sprintf(debug_str, "SDIO ClkDiv:%d FlowCtrl:%d", hsd.Init.ClockDiv, hsd.Init.HardwareFlowControl);
    lcd_show_string(10, 360, 300, 16, 12, debug_str, BLACK);
    
    sprintf(debug_str, "Bus Width:%s", (hsd.Init.BusWide == SDIO_BUS_WIDE_4B) ? "4bit" : "1bit");
    lcd_show_string(10, 380, 300, 16, 12, debug_str, BLACK);
    
    /* 显示SD卡状态 */
    HAL_SD_CardStateTypeDef cardState = HAL_SD_GetCardState(&hsd);
    const char* stateStr;
    switch(cardState)
    {
        case HAL_SD_CARD_READY:      stateStr = "READY"; break;
        case HAL_SD_CARD_IDENTIFICATION: stateStr = "IDENT"; break;
        case HAL_SD_CARD_STANDBY:    stateStr = "STANDBY"; break;
        case HAL_SD_CARD_TRANSFER:   stateStr = "TRANSFER"; break;
        case HAL_SD_CARD_SENDING:    stateStr = "SENDING"; break;
        case HAL_SD_CARD_RECEIVING:  stateStr = "RECEIVING"; break;
        case HAL_SD_CARD_PROGRAMMING: stateStr = "PROGRAMMING"; break;
        case HAL_SD_CARD_DISCONNECTED: stateStr = "DISCONNECTED"; break;
        case HAL_SD_CARD_ERROR:      stateStr = "ERROR"; break;
        default:                     stateStr = "UNKNOWN"; break;
    }
    sprintf(debug_str, "Card State: %s", stateStr);
    lcd_show_string(10, 400, 300, 16, 12, debug_str, BLACK);
}

/**
 * @brief       获取卡信息函数
 * @param       cardinfo:SD卡信息句柄
 * @retval      返回值:读取卡信息状态值
 */
uint8_t get_sd_card_info(HAL_SD_CardInfoTypeDef *cardinfo)
{
    uint8_t sta;
    sta = HAL_SD_GetCardInfo(&hsd, cardinfo);
    return sta;
}

/**
 * @brief       判断SD卡是否可以传输(读写)数据
 * @param       无
 * @retval      返回值:SD_TRANSFER_OK      传输完成，可以继续下一次传输
 *                     SD_TRANSFER_BUSY SD 卡正忙，不可以进行下一次传输
 */
uint8_t get_sd_card_state(void)
{
    return ((HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
}

/**
 * @brief       读SD卡(fatfs/usb调用)
 * @param       pbuf  : 数据缓存区
 * @param       saddr : 扇区地址
 * @param       cnt   : 扇区个数
 * @retval      0, 正常;  其他, 错误代码(详见SD_Error定义);
 */
uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
    uint8_t sta = HAL_OK;
    uint32_t timeout = SD_TIMEOUT;
    long long lsector = saddr;
    
    __disable_irq();    /* 关闭总中断(POLLING模式,严禁中断打断SDIO读写操作!!!) */
    
    sta = HAL_SD_ReadBlocks(&hsd, (uint8_t *)pbuf, lsector, cnt, SD_TIMEOUT);
    
    /* 等待SD卡读完 */
    while (get_sd_card_state() != SD_TRANSFER_OK)
    {
        if (timeout-- == 0)
        {
            sta = SD_TRANSFER_BUSY;
            break;
        }
    }
    
    __enable_irq(); /* 开启总中断 */
    return sta;
}

/**
 * @brief       写SD卡(fatfs/usb调用)
 * @param       pbuf  : 数据缓存区
 * @param       saddr : 扇区地址
 * @param       cnt   : 扇区个数
 * @retval      0, 正常;  其他, 错误代码(详见SD_Error定义);
 */
uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
    uint8_t sta = HAL_OK;
    uint32_t timeout = SD_TIMEOUT;
    long long lsector = saddr;
    
    __disable_irq();    /* 关闭总中断(POLLING模式,严禁中断打断SDIO读写操作!!!) */
    
    sta = HAL_SD_WriteBlocks(&hsd, (uint8_t *)pbuf, lsector, cnt, SD_TIMEOUT);
    
    /* 等待SD卡写完 */
    while (get_sd_card_state() != SD_TRANSFER_OK)
    {
        if (timeout-- == 0)
        {
            sta = SD_TRANSFER_BUSY;
            break;
        }
    }
    
    __enable_irq(); /* 开启总中断 */
    return sta;
}

/**
 * @brief       通过LCD显示SD卡相关信息
 * @param       无
 * @retval      无
 */
void show_sdcard_info(void)
{
    HAL_SD_CardCIDTypeDef sd_card_cid;
    char info_str[50];

    HAL_SD_GetCardCID(&hsd, &sd_card_cid);          /* 获取CID */
    get_sd_card_info(&g_sd_card_info_handle);       /* 获取SD卡信息 */

    /* 显示SD卡类型 */
    switch (g_sd_card_info_handle.CardType)
    {
        case CARD_SDSC:
            if (g_sd_card_info_handle.CardVersion == CARD_V1_X)
            {
                sprintf(info_str, "Card Type: SDSC V1");
            }
            else if (g_sd_card_info_handle.CardVersion == CARD_V2_X)
            {
                sprintf(info_str, "Card Type: SDSC V2");
            }
            break;

        case CARD_SDHC_SDXC:
            sprintf(info_str, "Card Type: SDHC/SDXC");
            break;

        default:
            sprintf(info_str, "Card Type: Unknown");
            break;
    }
    lcd_show_string(10, 180, 300, 16, 16, info_str, BLUE);

    /* 显示制造商ID */
    sprintf(info_str, "Manufacturer ID: %d", sd_card_cid.ManufacturerID);
    lcd_show_string(10, 200, 300, 16, 16, info_str, BLUE);

    /* 显示容量 */
    uint32_t capacity_mb = SD_TOTAL_SIZE_MB(&hsd);
    if (capacity_mb > 1024)
    {
        sprintf(info_str, "Capacity: %d.%d GB", capacity_mb / 1024, (capacity_mb % 1024) * 10 / 1024);
    }
    else
    {
        sprintf(info_str, "Capacity: %d MB", capacity_mb);
    }
    lcd_show_string(10, 220, 300, 16, 16, info_str, BLUE);

    /* 显示块大小 */
    sprintf(info_str, "Block Size: %d bytes", g_sd_card_info_handle.BlockSize);
    lcd_show_string(10, 240, 300, 16, 16, info_str, BLUE);
}

/**
 * @brief       测试SD卡的读取并在LCD上显示结果
 * @param       secaddr : 扇区地址
 * @param       seccnt  : 扇区数
 * @retval      0, 成功; 其他, 失败
 */
uint8_t sd_test_read(uint32_t secaddr, uint32_t seccnt)
{
    uint8_t *buf;
    uint8_t sta = 0;
    char result_str[50];

    /* 简单的内存分配，避免复杂的内存管理 */
    static uint8_t test_buf[512];   /* 静态缓冲区，测试1个扇区 */
    
    if (seccnt > 1) seccnt = 1;     /* 限制为1个扇区，避免内存不足 */
    buf = test_buf;

    lcd_show_string(10, 300, 300, 16, 16, "Reading SD card...", BLUE);
    
    sta = sd_read_disk(buf, secaddr, seccnt);
    
    if (sta == 0)
    {
        sprintf(result_str, "Read sector %d: SUCCESS", secaddr);
        lcd_show_string(10, 320, 300, 16, 16, result_str, GREEN);
        
        /* 显示前16字节的数据作为示例 */
        sprintf(result_str, "Data: %02X %02X %02X %02X %02X %02X %02X %02X", 
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
        lcd_show_string(10, 340, 300, 16, 16, result_str, BLACK);
    }
    else
    {
        sprintf(result_str, "Read sector %d: FAILED (err:%d)", secaddr, sta);
        lcd_show_string(10, 320, 300, 16, 16, result_str, RED);
    }

    return sta;
} 