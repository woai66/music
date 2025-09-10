/**
 ****************************************************************************************************
 * @file        hr2046.h
 * @author      正点原子团队(ALIENTEK) 移植适配
 * @version     V1.0
 * @date        2024-12-19
 * @brief       XPT2046触摸屏驱动头文件
 * @note        XPT2046是4线电阻触摸屏控制器，兼容ADS7846
 ****************************************************************************************************
 */

#ifndef HR2046_H
#define HR2046_H

#include <stdbool.h>
#include "main.h"

/* ============================================================================
 * 引脚定义 - 根据正点原子战舰V3开发板实际连接
 * ============================================================================ */

/* T_PEN引脚定义 (PF10) - 触摸检测引脚 */
#define T_PEN_GPIO_PORT                 GPIOF
#define T_PEN_GPIO_PIN                  GPIO_PIN_10
#define T_PEN_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)

/* T_CS引脚定义 (PF11) - 片选引脚 */
#define T_CS_GPIO_PORT                  GPIOF
#define T_CS_GPIO_PIN                   GPIO_PIN_11
#define T_CS_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)

/* T_MISO引脚定义 (PB2) - 数据输入引脚 */
#define T_MISO_GPIO_PORT                GPIOB
#define T_MISO_GPIO_PIN                 GPIO_PIN_2
#define T_MISO_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

/* T_MOSI引脚定义 (PF9) - 数据输出引脚 */
#define T_MOSI_GPIO_PORT                GPIOF
#define T_MOSI_GPIO_PIN                 GPIO_PIN_9
#define T_MOSI_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)

/* T_CLK引脚定义 (PB1) - 时钟引脚 */
#define T_CLK_GPIO_PORT                 GPIOB
#define T_CLK_GPIO_PIN                  GPIO_PIN_1
#define T_CLK_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

/* ============================================================================
 * 引脚操作宏定义
 * ============================================================================ */

/* 触摸检测引脚读取 */
#define T_PEN           HAL_GPIO_ReadPin(T_PEN_GPIO_PORT, T_PEN_GPIO_PIN)

/* 数据输入引脚读取 */
#define T_MISO          HAL_GPIO_ReadPin(T_MISO_GPIO_PORT, T_MISO_GPIO_PIN)

/* 数据输出引脚控制 */
#define T_MOSI(x)       do{ x ? \
                          HAL_GPIO_WritePin(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, GPIO_PIN_RESET); \
                        }while(0)

/* 时钟引脚控制 */
#define T_CLK(x)        do{ x ? \
                          HAL_GPIO_WritePin(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, GPIO_PIN_RESET); \
                        }while(0)

/* 片选引脚控制 */
#define T_CS(x)         do{ x ? \
                          HAL_GPIO_WritePin(T_CS_GPIO_PORT, T_CS_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_CS_GPIO_PORT, T_CS_GPIO_PIN, GPIO_PIN_RESET); \
                        }while(0)

/* ============================================================================
 * 触摸屏相关宏定义
 * ============================================================================ */

#define TP_PRES_DOWN    0x8000    /* 触屏被按下 */  
#define TP_CATH_PRES    0x4000    /* 有效的按下 */
#define CT_MAX_TOUCH    10        /* 电容屏支持的点数,固定为10点 */

/* 触摸屏控制器类型 */
#define TP_TYPE_CAPACITIVE  0   /* 电容屏 */
#define TP_TYPE_RESISTIVE   1   /* 电阻屏 */

/* ============================================================================
 * 结构体定义
 * ============================================================================ */

/* 触摸屏控制器结构体 */
typedef struct
{
    uint8_t (*init)(void);          /* 初始化触摸屏控制器 */
    uint8_t (*scan)(uint8_t);       /* 扫描触摸屏，0:屏幕坐标; 1:物理坐标 */
    void (*adjust)(void);           /* 触摸屏校准 */
    uint16_t x[CT_MAX_TOUCH];       /* 当前坐标 */
    uint16_t y[CT_MAX_TOUCH];       /* 电容屏有最多10个触摸点,电阻屏只有1个 */
    uint16_t sta;                   /* 笔的状态 */
                                    /* b15:按下1/松开0; 
                                     * b14:0,没有按键按下;1,有按键按下. 
                                     * b13~b10:保留
                                     * b9~b0:电容触摸屏按下的点数(0,表示未按下,1表示按下)
                                     */
    
    /* 5点校准触摸屏校准参数(电容屏不需要校准) */
    float xfac;                     /* 5点校准法x方向比例因子 */
    float yfac;                     /* 5点校准法y方向比例因子 */
    short xc;                       /* 中心X坐标物理值(AD值) */
    short yc;                       /* 中心Y坐标物理值(AD值) */
    
    /* 新增的参数,当触摸屏的左右上下完全颠倒时需要用到.
     * b0:0, 竖屏(适合左右为X坐标,上下为Y坐标的TP)
     *    1, 横屏(适合左右为Y坐标,上下为X坐标的TP)
     * b1~6: 保留.
     * b7:0, 电阻屏
     *    1, 电容屏
     */
    uint8_t touchtype;
}_m_tp_dev;

/* ============================================================================
 * 全局变量声明
 * ============================================================================ */

extern _m_tp_dev tp_dev;           /* 触摸屏控制器在hr2046.c里面定义 */

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/* 基础函数 */
void delay_us(uint32_t us);                                    /* 微秒延时函数 */
uint8_t tp_init(void);                                         /* 初始化 */
uint8_t tp_scan(uint8_t mode);                                 /* 扫描触摸屏 */
void tp_adjust(void);                                          /* 触摸屏校准 */

/* 测试和调试函数 */
void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color);    /* 画大点(用于测试) */

/* 校准参数保存和读取 */
void tp_save_adjust_data(void);     /* 保存校准参数 */
uint8_t tp_get_adjust_data(void);   /* 读取校准参数 */

/* 调试函数 */
void tp_read_xy(uint16_t *x, uint16_t *y);  /* 读取原始坐标 */
uint16_t tp_test_spi(void);                 /* 测试SPI通信 */

/* 兼容性函数 */
bool hr2046_read(uint16_t *x, uint16_t *y, uint16_t *z);          /* 兼容旧接口 */

#endif /* HR2046_H */ 