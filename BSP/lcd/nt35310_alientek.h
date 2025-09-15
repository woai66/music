#ifndef __NT35310_ALIENTEK_H
#define __NT35310_ALIENTEK_H

#include "main.h"
#include "stdbool.h"

/* 
 * ============================================================================
 * LCD驱动 - 基于正点原子ALIENTEK代码结构
 * ============================================================================
 * 硬件连接:
 * - FSMC_NE4 (PG12) -> LCD_CS   (片选信号)
 * - FSMC_A10 (PG0)  -> LCD_RS   (寄存器选择，关键！必须是A10)
 * - FSMC_NOE (PD4)  -> LCD_RD   (读信号)
 * - FSMC_NWE (PD5)  -> LCD_WR   (写信号)
 * - FSMC_D0~D15     -> LCD_D0~D15 (16位数据总线)
 * - PB0             -> LCD_BL   (背光控制)
 * 
 * 关键配置:
 * - 使用FSMC_NE4 (第4个片选)
 * - 使用FSMC_A10作为RS线 (这是成功的关键！)
 * - 16位数据宽度
 * - LCD Interface模式
 * ============================================================================
 */

/* LCD重要参数集 */
typedef struct
{
    uint16_t width;     /* LCD 宽度 */
    uint16_t height;    /* LCD 高度 */
    uint16_t id;        /* LCD ID */
    uint8_t dir;        /* 横屏还是竖屏控制：0，竖屏；1，横屏。 */
    uint16_t wramcmd;   /* 开始写gram指令 */
    uint16_t setxcmd;   /* 设置x坐标指令 */
    uint16_t setycmd;   /* 设置y坐标指令 */
} _lcd_dev;

/* LCD参数 */
extern _lcd_dev lcddev; /* 管理LCD重要参数 */

/* LCD的画笔颜色和背景色 */
extern uint16_t g_point_color;     /* 画笔颜色，默认红色 */
extern uint16_t g_back_color;      /* 背景颜色，默认为白色 */

/* LCD背光控制 - 使用PB0 */
#define LCD_BL(x)   do{ x ? \
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); \
                     }while(0)

/* LCD地址结构体 - 完全按照正点原子的方式 */
typedef struct
{
    volatile uint16_t LCD_REG;
    volatile uint16_t LCD_RAM;
} LCD_TypeDef;

/* 
 * FSMC相关参数定义 - 关键配置
 * 注意: 这些参数必须与CubeMX配置完全匹配！
 */
#define LCD_FSMC_NEX         4              /* 使用FSMC_NE4接LCD_CS */
#define LCD_FSMC_AX          10             /* 使用FSMC_A10接LCD_RS (关键！) */

/* 
 * LCD_BASE计算 - 完全按照正点原子的公式
 * 计算公式: 基地址 + 片选偏移 + RS偏移
 * - 基地址: 0x60000000 (FSMC Bank1)
 * - 片选偏移: 0x4000000 * (NE4-1) = 0xC000000
 * - RS偏移: (1<<A10)*2 - 2 = 2048*2 - 2 = 4094 = 0xFFE
 * 最终: 0x6C000000 | 0xFFE = 0x6C000FFE
 */
#define LCD_BASE        (uint32_t)((0X60000000 + (0X4000000 * (LCD_FSMC_NEX - 1))) | (((1 << LCD_FSMC_AX) * 2) -2))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

/* 扫描方向定义 */
#define L2R_U2D         0           /* 从左到右,从上到下 */
#define L2R_D2U         1           /* 从左到右,从下到上 */
#define R2L_U2D         2           /* 从右到左,从上到下 */
#define R2L_D2U         3           /* 从右到左,从下到上 */

#define U2D_L2R         4           /* 从上到下,从左到右 */
#define U2D_R2L         5           /* 从上到下,从右到左 */
#define D2U_L2R         6           /* 从下到上,从左到右 */
#define D2U_R2L         7           /* 从下到上,从右到左 */

#define DFT_SCAN_DIR    L2R_U2D     /* 默认的扫描方向 */

/* 常用画笔颜色 */
#define WHITE           0xFFFF      /* 白色 */
#define BLACK           0x0000      /* 黑色 */
#define RED             0xF800      /* 红色 */
#define GREEN           0x07E0      /* 绿色 */
#define BLUE            0x001F      /* 蓝色 */ 
#define MAGENTA         0XF81F      /* 品红色/紫红色 = BLUE + RED */
#define YELLOW          0XFFE0      /* 黄色 = GREEN + RED */
#define CYAN            0X07FF      /* 青色 = GREEN + BLUE */

/* 函数声明 */
void lcd_wr_data(volatile uint16_t data);            /* LCD写数据 */
void lcd_wr_regno(volatile uint16_t regno);          /* LCD写寄存器编号/地址 */
void lcd_write_reg(uint16_t regno, uint16_t data);   /* LCD写寄存器的值 */

void lcd_init(void);                        /* 初始化LCD */ 
void lcd_display_on(void);                  /* 开显示 */ 
void lcd_display_off(void);                 /* 关显示 */
void lcd_scan_dir(uint8_t dir);             /* 设置屏扫描方向 */ 
void lcd_display_dir(uint8_t dir);          /* 设置屏幕显示方向 */ 

void lcd_write_ram_prepare(void);               /* 准备些GRAM */ 
void lcd_set_cursor(uint16_t x, uint16_t y);    /* 设置光标 */ 
uint32_t lcd_read_point(uint16_t x, uint16_t y);/* 读点(32位颜色,兼容LTDC)  */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color);/* 画点(32位颜色,兼容LTDC) */

void lcd_clear(uint16_t color);     /* LCD清屏 */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);          /* 纯色填充矩形 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);     /* 画直线 */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);/* 画矩形 */

void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);                       /* 显示一个字符 */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);   /* 显示字符串 */

/* 界面管理函数 */
void lcd_draw_standard_ui(const char* status_text);    /* 绘制标准界面 */
void lcd_show_splash_screen(void);                     /* 显示启动界面 */
void lcd_show_device_info(void);                       /* 显示设备信息 */

/* 设备初始化管理函数 */
uint8_t device_init_all(void);                        /* 初始化所有设备 */
uint8_t device_init_touch(bool force_calibration);    /* 初始化触摸屏 */
uint8_t device_init_sdcard(void);                     /* 初始化SD卡 */
uint8_t device_init_filesystem(void);                 /* 初始化文件系统 */
uint8_t device_init_audio(void);                      /* 初始化音频设备 */

/* NT35310初始化序列 */
void lcd_ex_nt35310_reginit(void);

#endif 