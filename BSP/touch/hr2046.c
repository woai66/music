/**
 ****************************************************************************************************
 * @file        hr2046.c
 * @author      正点原子团队(ALIENTEK) 移植适配
 * @version     V1.0
 * @date        2024-12-19
 * @brief       XPT2046触摸屏驱动代码
 * @note        XPT2046是4线电阻触摸屏控制器，兼容ADS7846
 *              支持触摸检测、坐标读取、五点校准等功能
 ****************************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include "hr2046.h"
#include "nt35310_alientek.h"

/* ============================================================================
 * 全局变量定义
 * ============================================================================ */
_m_tp_dev tp_dev = 
{
    tp_init,
    tp_scan,
    tp_adjust,
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* x坐标数组 */
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* y坐标数组 */
    0,                                /* sta状态 */
    0,                                /* xfac */
    0,                                /* yfac */
    0,                                /* xc */
    0,                                /* yc */
    0,                                /* touchtype */
};

/* ============================================================================
 * 底层SPI通信函数
 * ============================================================================ */

/**
 * @brief       控制触摸屏速度的延时
 * @param       无
 * @retval      无
 */
void delay_us(uint32_t us)
{
    uint32_t i;
    /* 72MHz系统时钟下，优化延时 */
    for (i = 0; i < us * 2; i++)  /* 减少延时倍数，提高响应速度 */
    {
        __NOP();
    }
}

/**
 * @brief       SPI写数据
 * @note        向触摸屏IC写入1 byte数据
 * @param       data: 要写入的数据
 * @retval      无
 */
static void tp_write_byte(uint8_t data)
{
    uint8_t count = 0;

    for (count = 0; count < 8; count++)
    {
        if (data & 0x80)    /* 发送1 */
        {
            T_MOSI(1);
        }
        else                /* 发送0 */
        {
            T_MOSI(0);
        }

        data <<= 1;
        T_CLK(0);
        delay_us(1);        /* 恢复时钟延时，确保稳定 */
        T_CLK(1);           /* 上升沿有效 */
        delay_us(1);        /* 恢复时钟延时，确保稳定 */
    }
}

/**
 * @brief       SPI读数据
 * @note        从触摸屏IC读取adc值
 * @param       cmd: 指令
 * @retval      读取到的数据,ADC值(12bit)
 */
static uint16_t tp_read_ad(uint8_t cmd)
{
    uint8_t count = 0;
    uint16_t num = 0;
    T_CLK(0);           /* 先拉低时钟 */
    T_MOSI(0);          /* 拉低数据线 */
    T_CS(0);            /* 选中触摸屏IC */
    tp_write_byte(cmd); /* 发送命令字 */
    delay_us(6);        /* 恢复转换等待时间，确保ADC稳定 */
    T_CLK(0);
    delay_us(1);
    T_CLK(1);           /* 给1个时钟，清除BUSY */
    delay_us(1);
    T_CLK(0);

    for (count = 0; count < 16; count++)    /* 读出16位数据,只有高12位有效 */
    {
        num <<= 1;
        T_CLK(0);       /* 下降沿有效 */
        delay_us(1);
        T_CLK(1);
        delay_us(1);
        if (T_MISO) num++;
    }

    num >>= 4;          /* 只有高12位有效. */
    T_CS(1);            /* 释放片选 */
    return num;
}

/* 电阻触摸驱动芯片 数据采集 滤波用参数 */
#define TP_READ_TIMES   5       /* 读取次数 */
#define TP_LOST_VAL     1       /* 丢弃值 */

/**
 * @brief       读取一个坐标值(x或者y)
 * @note        连续读取TP_READ_TIMES次数据,对这些数据升序排列,
 *              然后去掉最低和最高TP_LOST_VAL个数, 取平均值
 * @param       cmd : 指令
 * @retval      读取到的数据(滤波后的), ADC值(12bit)
 */
static uint16_t tp_read_xoy(uint8_t cmd)
{
    uint16_t i, j;
    uint16_t buf[TP_READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;

    for (i = 0; i < TP_READ_TIMES; i++)   /* 先读取TP_READ_TIMES次数据 */
    {
        buf[i] = tp_read_ad(cmd);
    }

    for (i = 0; i < TP_READ_TIMES - 1; i++)   /* 对数据进行排序 */
    {
        for (j = i + 1; j < TP_READ_TIMES; j++)
        {
            if (buf[i] > buf[j])   /* 升序排列 */
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }

    sum = 0;
    for (i = TP_LOST_VAL; i < TP_READ_TIMES - TP_LOST_VAL; i++)   /* 去掉两端的丢弃值 */
    {
        sum += buf[i];  /* 累加去掉丢弃值以后的数据. */
    }

    temp = sum / (TP_READ_TIMES - 2 * TP_LOST_VAL); /* 取平均值 */
    return temp;
}

/**
 * @brief       读取x, y坐标
 * @param       x,y: 读取到的坐标值
 * @retval      无
 */
void tp_read_xy(uint16_t *x, uint16_t *y)
{
    uint16_t xval, yval;

    if (tp_dev.touchtype & 0X01)    /* X,Y方向与屏幕相反 */
    {
        xval = tp_read_xoy(0X90);   /* 读取X轴坐标AD值, 并进行方向变换 */
        yval = tp_read_xoy(0XD0);   /* 读取Y轴坐标AD值 */
    }
    else                            /* X,Y方向与屏幕相同 */
    {
        xval = tp_read_xoy(0XD0);   /* 读取X轴坐标AD值 */
        yval = tp_read_xoy(0X90);   /* 读取Y轴坐标AD值 */
    }

    *x = xval;
    *y = yval;
}

/* 连续两次读取X,Y坐标的数据误差最大允许值 */
#define TP_ERR_RANGE    50      /* 误差范围 */

/**
 * @brief       连续读取2次触摸IC数据, 并滤波
 * @param       x,y: 读取到的坐标值
 * @retval      0, 失败; 1, 成功;
 */
static uint8_t tp_read_xy2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1;
    uint16_t x2, y2;

    tp_read_xy(&x1, &y1);   /* 读取第一次数据 */
    tp_read_xy(&x2, &y2);   /* 读取第二次数据 */

    /* 前后两次采样在+-TP_ERR_RANGE内 */
    if (((x2 <= x1 && x1 < x2 + TP_ERR_RANGE) || (x1 <= x2 && x2 < x1 + TP_ERR_RANGE)) &&
        ((y2 <= y1 && y1 < y2 + TP_ERR_RANGE) || (y1 <= y2 && y2 < y1 + TP_ERR_RANGE)))
    {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    }

    return 0;
}

/**
 * @brief       测试SPI通信
 * @note        直接读取ADC值，用于调试
 * @retval      读取到的ADC值
 */
uint16_t tp_test_spi(void)
{
    return tp_read_ad(0XD0);  /* 读取X轴ADC值 */
}

/* ============================================================================
 * 外部接口函数
 * ============================================================================ */

/**
 * @brief       触摸按键扫描
 * @param       mode: 坐标模式
 *              0, 屏幕坐标;
 *              1, 物理坐标(校准等特殊场合用)
 * @retval      0, 触屏无触摸; 1, 触屏有触摸;
 */
uint8_t tp_scan(uint8_t mode)
{
    if (T_PEN == 0)     /* 有按键按下 */
    {
        if (mode)       /* 读取物理坐标, 无需转换 */
        {
            tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]);
        }
        else if (tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]))     /* 读取屏幕坐标, 需要转换 */
        {
            /* 将X轴 物理坐标转换成逻辑坐标(即对应LCD屏幕上面的X坐标值) */
            tp_dev.x[0] = (signed short)(tp_dev.x[0] - tp_dev.xc) / tp_dev.xfac + lcddev.width / 2;

            /* 将Y轴 物理坐标转换成逻辑坐标(即对应LCD屏幕上面的Y坐标值) */
            tp_dev.y[0] = (signed short)(tp_dev.y[0] - tp_dev.yc) / tp_dev.yfac + lcddev.height / 2;
        }

        if ((tp_dev.sta & TP_PRES_DOWN) == 0)   /* 之前没有被按下 */
        {
            tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES;   /* 按键按下 */
            tp_dev.x[CT_MAX_TOUCH - 1] = tp_dev.x[0];   /* 记录第一次按下时的坐标 */
            tp_dev.y[CT_MAX_TOUCH - 1] = tp_dev.y[0];
        }
    }
    else
    {
        if (tp_dev.sta & TP_PRES_DOWN)      /* 之前是被按下的 */
        {
            tp_dev.sta &= ~TP_PRES_DOWN;    /* 标记按键松开 */
        }
        else     /* 之前就没有被按下 */
        {
            tp_dev.x[CT_MAX_TOUCH - 1] = 0;
            tp_dev.y[CT_MAX_TOUCH - 1] = 0;
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
        }
    }

    return tp_dev.sta & TP_PRES_DOWN; /* 返回当前的触屏状态 */
}

/**
 * @brief       画一个大点(2*2的点)
 * @param       x,y   : 坐标
 * @param       color : 颜色
 * @retval      无
 */
void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_point(x, y, color);       /* 中心点 */
    lcd_draw_point(x + 1, y, color);
    lcd_draw_point(x, y + 1, color);
    lcd_draw_point(x + 1, y + 1, color);
}

/**
 * @brief       画一个校准用的触摸点(十字架)
 * @param       x,y   : 坐标
 * @param       color : 颜色
 * @retval      无
 */
static void tp_draw_touch_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_line(x - 12, y, x + 13, y, color); /* 横线 */
    lcd_draw_line(x, y - 12, x, y + 13, color); /* 竖线 */
    lcd_draw_point(x + 1, y + 1, color);
    lcd_draw_point(x - 1, y + 1, color);
    lcd_draw_point(x + 1, y - 1, color);
    lcd_draw_point(x - 1, y - 1, color);
}

/**
 * @brief       正点原子的5点校准法
 * @note        使用五点校准法，得到准确的触摸坐标转换参数
 */
void tp_adjust(void)
{
    uint16_t pxy[5][2];     /* 物理坐标缓存值 */
    uint8_t  cnt = 0;
    short s1, s2, s3, s4;   /* 4个点的坐标差值 */
    double px, py;          /* X,Y轴物理坐标比例,用于判定是否校准成功 */
    uint16_t outtime = 0;

    /* 保存当前背景色，设置白色背景用于校准 */
    uint16_t old_back_color = g_back_color;
    g_back_color = WHITE;
    
    lcd_clear(WHITE);       /* 清屏 */
    lcd_show_string(40, 40, 280, 100, 16, "Please touch the cross", RED); /* 显示提示信息 */
    lcd_show_string(40, 60, 280, 100, 16, "on the screen to calibrate", RED); 
    tp_draw_touch_point(20, 20, RED);   /* 画点1 */
    tp_dev.sta = 0;         /* 消除触发信号 */

    while (1)               /* 如果连续10秒钟没有按下,则自动退出 */
    {
        tp_dev.scan(1);     /* 扫描物理坐标 */

        if ((tp_dev.sta & 0xc000) == TP_CATH_PRES)   /* 按键按下了一次(此时按键松开了.) */
        {
            outtime = 0;
            tp_dev.sta &= ~TP_CATH_PRES;    /* 标记按键已经被处理过了. */

            pxy[cnt][0] = tp_dev.x[0];      /* 保存X物理坐标 */
            pxy[cnt][1] = tp_dev.y[0];      /* 保存Y物理坐标 */
            cnt++;

            switch (cnt)
            {
                case 1:
                    tp_draw_touch_point(20, 20, WHITE);                 /* 清除点1 */
                    tp_draw_touch_point(lcddev.width - 20, 20, RED);    /* 画点2 */
                    break;

                case 2:
                    tp_draw_touch_point(lcddev.width - 20, 20, WHITE);  /* 清除点2 */
                    tp_draw_touch_point(20, lcddev.height - 20, RED);   /* 画点3 */
                    break;

                case 3:
                    tp_draw_touch_point(20, lcddev.height - 20, WHITE); /* 清除点3 */
                    tp_draw_touch_point(lcddev.width - 20, lcddev.height - 20, RED);    /* 画点4 */
                    break;

                case 4:
                    lcd_clear(WHITE);   /* 画第五个点了, 直接清屏 */
                    tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, RED);  /* 画点5 */
                    break;

                case 5:     /* 全部5个点已经得到 */
                    s1 = pxy[1][0] - pxy[0][0]; /* 第2个点和第1个点的X轴物理坐标差值(AD值) */
                    s3 = pxy[3][0] - pxy[2][0]; /* 第4个点和第3个点的X轴物理坐标差值(AD值) */
                    s2 = pxy[3][1] - pxy[1][1]; /* 第4个点和第2个点的Y轴物理坐标差值(AD值) */
                    s4 = pxy[2][1] - pxy[0][1]; /* 第3个点和第1个点的Y轴物理坐标差值(AD值) */

                    px = (double)s1 / s3;       /* X轴比例因子 */
                    py = (double)s2 / s4;       /* Y轴比例因子 */

                    if (px < 0) px = -px;        /* 负数改正数 */
                    if (py < 0) py = -py;        /* 负数改正数 */

                    if (px < 0.95 || px > 1.05 || py < 0.95 || py > 1.05 ||     /* 比例不合格 */
                            abs(s1) > 4095 || abs(s2) > 4095 || abs(s3) > 4095 || abs(s4) > 4095 || /* 差值不合格, 大于坐标范围 */
                            abs(s1) == 0 || abs(s2) == 0 || abs(s3) == 0 || abs(s4) == 0            /* 差值不合格, 等于0 */
                       )
                    {
                        cnt = 0;
                        tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, WHITE); /* 清除点5 */
                        tp_draw_touch_point(20, 20, RED);   /* 重新画点1 */
                        lcd_show_string(40, 120, 280, 16, 12, "Calibration failed! Retry...", RED);
                        HAL_Delay(1000);
                        lcd_clear(WHITE);
                        lcd_show_string(40, 40, 280, 100, 16, "Please touch the cross", RED);
                        lcd_show_string(40, 60, 280, 100, 16, "on the screen to calibrate", RED);
                        continue;
                    }

                    tp_dev.xfac = (float)(s1 + s3) / (2 * (lcddev.width - 40));
                    tp_dev.yfac = (float)(s2 + s4) / (2 * (lcddev.height - 40));

                    tp_dev.xc = pxy[4][0];      /* X轴,物理中心坐标 */
                    tp_dev.yc = pxy[4][1];      /* Y轴,物理中心坐标 */

                    lcd_clear(WHITE);   /* 清屏 */
                    lcd_show_string(35, 110, lcddev.width, lcddev.height, 16, "Touch Screen Adjust OK!", BLUE); /* 校正完成 */
                    HAL_Delay(1000);
                    
                    /* 自动保存校准参数 */
                    tp_save_adjust_data();

                    /* 恢复背景色并清屏 */
                    g_back_color = old_back_color;
                    lcd_clear(g_back_color);/* 清屏为原来的背景色 */
                    return;/* 校正完成 */
            }
        }

        HAL_Delay(10);
        outtime++;

        if (outtime > 1000)  /* 10秒超时 */
        {
            /* 使用默认校准参数 */
            tp_dev.xfac = -0.074f;
            tp_dev.yfac = 0.098f;  
            tp_dev.xc = 2048;
            tp_dev.yc = 2048;
            
            /* 恢复背景色并清屏 */
            g_back_color = old_back_color;
            lcd_clear(g_back_color);
            break;
        }
    }
}

/**
 * @brief       触摸屏初始化
 * @param       无
 * @retval      0,没有进行校准
 *              1,进行过校准
 */
uint8_t tp_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    
    tp_dev.touchtype = 0;                   /* 默认设置(电阻屏 & 竖屏) */
    tp_dev.touchtype |= lcddev.dir & 0X01;  /* 根据LCD判定是横屏还是竖屏 */

    /* 对于NT35310(0x5310) ID的屏幕，使用电阻屏初始化 */
    if (lcddev.id == 0X5310)
    {
        /* 使能各个引脚的时钟 */
        T_PEN_GPIO_CLK_ENABLE();    /* T_PEN脚时钟使能 */
        T_CS_GPIO_CLK_ENABLE();     /* T_CS脚时钟使能 */
        T_MISO_GPIO_CLK_ENABLE();   /* T_MISO脚时钟使能 */
        T_MOSI_GPIO_CLK_ENABLE();   /* T_MOSI脚时钟使能 */
        T_CLK_GPIO_CLK_ENABLE();    /* T_CLK脚时钟使能 */

        /* 配置T_PEN引脚 (PF10) */
        gpio_init_struct.Pin = T_PEN_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_INPUT;                 /* 输入 */
        gpio_init_struct.Pull = GPIO_PULLUP;                     /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;           /* 高速 */
        HAL_GPIO_Init(T_PEN_GPIO_PORT, &gpio_init_struct);       /* 初始化T_PEN引脚 */

        /* 配置T_MISO引脚 (PB2) */
        gpio_init_struct.Pin = T_MISO_GPIO_PIN;
        HAL_GPIO_Init(T_MISO_GPIO_PORT, &gpio_init_struct);      /* 初始化T_MISO引脚 */

        /* 配置T_MOSI引脚 (PF9) */
        gpio_init_struct.Pin = T_MOSI_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;             /* 推挽输出 */
        gpio_init_struct.Pull = GPIO_PULLUP;                     /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;           /* 高速 */
        HAL_GPIO_Init(T_MOSI_GPIO_PORT, &gpio_init_struct);      /* 初始化T_MOSI引脚 */

        /* 配置T_CLK引脚 (PB1) */
        gpio_init_struct.Pin = T_CLK_GPIO_PIN;
        HAL_GPIO_Init(T_CLK_GPIO_PORT, &gpio_init_struct);       /* 初始化T_CLK引脚 */

        /* 配置T_CS引脚 (PF11) */
        gpio_init_struct.Pin = T_CS_GPIO_PIN;
        HAL_GPIO_Init(T_CS_GPIO_PORT, &gpio_init_struct);        /* 初始化T_CS引脚 */

        /* 设置初始引脚状态 */
        T_CS(1);    /* 片选拉高 */
        T_CLK(1);   /* 时钟拉高 */
        T_MOSI(0);  /* MOSI拉低 */

        /* 初始化触摸屏 */
        tp_read_xy(&tp_dev.x[0], &tp_dev.y[0]); /* 第一次读取初始化 */
        
        /* 检查是否存在校准数据 */
        if (tp_get_adjust_data() == 0)
        {
            /* 不存在校准数据，进行校准 */
            tp_adjust();
            /* 保存校准数据 */
            tp_save_adjust_data();
        }
        else
        {
            /* 存在校准数据，加载校准参数 */
            lcd_clear(WHITE);
            lcd_show_string(40, 40, 280, 100, 16, "Touch Screen Ready!", BLUE);
            HAL_Delay(1000);
            /* 恢复背景色 */
            lcd_clear(g_back_color);
        }
        
        return 0;           /* 初始化完成 */
    }

    /* 如果不是NT35310屏幕，返回错误 */
    tp_dev.touchtype = 0XFF;  /* 标记为未检测到 */
    return 1;
}

/* ============================================================================
 * 兼容性函数
 * ============================================================================ */

/**
 * @brief       兼容性函数 - 读取触摸屏数据
 * @param       x,y,z: 读取到的坐标和压力值
 * @retval      读取结果 true:成功 false:失败
 * @note        为了兼容旧代码而保留的接口
 */
bool hr2046_read(uint16_t *x, uint16_t *y, uint16_t *z)
{
    if (tp_scan(0))  /* 扫描触摸屏 */
    {
        if (x) *x = tp_dev.x[0];
        if (y) *y = tp_dev.y[0];
        if (z) *z = 100;  /* 简化实现，固定压力值 */
        return true;
    }
    return false;
} 

/* 触摸屏校准参数保存在Flash的地址（最后一页） */
#define TP_SAVE_ADDR_BASE   0x0807F000  /* STM32F103ZET6的最后一页地址 */
#define TP_SAVE_FLAG        0x0A55      /* 校准标志 */

/**
 * @brief       保存校准参数到Flash
 * @note        将校准参数保存在Flash的最后一页
 * @param       无
 * @retval      无
 */
void tp_save_adjust_data(void)
{
    uint32_t data[4];
    
    /* 准备要保存的数据 */
    data[0] = *(uint32_t*)&tp_dev.xfac;  /* xfac */
    data[1] = *(uint32_t*)&tp_dev.yfac;  /* yfac */
    data[2] = (uint32_t)tp_dev.xc;       /* xc */
    data[3] = (uint32_t)tp_dev.yc;       /* yc */
    
    /* 解锁Flash */
    HAL_FLASH_Unlock();
    
    /* 擦除最后一页 */
    FLASH_EraseInitTypeDef erase_struct;
    uint32_t page_error = 0;
    
    erase_struct.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_struct.PageAddress = TP_SAVE_ADDR_BASE;
    erase_struct.NbPages = 1;
    
    HAL_FLASHEx_Erase(&erase_struct, &page_error);
    
    /* 写入校准标志 */
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, TP_SAVE_ADDR_BASE, TP_SAVE_FLAG);
    
    /* 写入校准参数 */
    for (int i = 0; i < 4; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, TP_SAVE_ADDR_BASE + 4 + i * 4, data[i]);
    }
    
    /* 锁定Flash */
    HAL_FLASH_Lock();
}

/**
 * @brief       从Flash读取校准参数
 * @param       无
 * @retval      0: 读取失败，需要重新校准
 *              1: 读取成功
 */
uint8_t tp_get_adjust_data(void)
{
    uint16_t flag = *(uint16_t*)TP_SAVE_ADDR_BASE;
    
    if (flag == TP_SAVE_FLAG)
    {
        /* 读取校准参数 */
        uint32_t* data = (uint32_t*)(TP_SAVE_ADDR_BASE + 4);
        
        tp_dev.xfac = *(float*)&data[0];
        tp_dev.yfac = *(float*)&data[1]; 
        tp_dev.xc = (uint16_t)data[2];
        tp_dev.yc = (uint16_t)data[3];
        
        return 1;  /* 校准参数有效 */
    }
    
    return 0;  /* 没有有效的校准参数 */
} 