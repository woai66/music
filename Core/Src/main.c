/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "sdio.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include "nt35310_alientek.h"
#include "hr2046.h"
#include "vs1053_port.h"
#include "lcdfont.h"
#include "sdio_sdcard.h"

// External function declaration
void lcd_basic_test(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief       绘制标准界面
 * @param       status_text: 状态文字
 * @retval      无
 */
void draw_standard_ui(const char* status_text)
{
  /* 确保背景色为白色 */
  g_back_color = WHITE;
  lcd_clear(WHITE);
  
  /* 绘制标准界面 */
  lcd_show_string(10, 10, 300, 32, 16, "STM32 Music Player", BLACK);
  lcd_show_string(10, 90, 300, 32, 12, status_text, BLUE);
  lcd_draw_rectangle(5, 120, 315, 470, BLACK);
  lcd_show_string(10, 130, 300, 32, 12, "Touch to draw", BLACK);
  lcd_show_string(10, 150, 300, 32, 12, "KEY0: Re-calibrate", BLACK);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FSMC_Init();
  MX_SDIO_SD_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 初始化LCD - 使用正点原子的方式 */
  lcd_init();
  
  /* 设置背景色为白色 */
  g_back_color = WHITE;
  
  /* 清屏为白色 */
  lcd_clear(WHITE);
  
  /* 显示文字测试 - 使用真正的字体 */
  lcd_show_string(10, 10, 300, 32, 16, "STM32 Music Player", BLACK);
  lcd_show_string(10, 40, 300, 32, 16, "Initializing...", BLUE);
  
  /* 显示LCD ID信息 */
  char text[30];
  sprintf((char *)text, "LCD ID: 0x%X", lcddev.id);
  lcd_show_string(10, 70, 300, 32, 12, text, RED);
  
  /* 检查是否需要强制校准 (KEY0按下) */
  bool force_calibration = false;
  if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)  /* KEY0按下 */
  {
    force_calibration = true;
    lcd_show_string(10, 90, 300, 32, 12, "Force Calibration", RED);
    HAL_Delay(1000);
  }
  
  /* 初始化触摸屏 */
  if (force_calibration)
  {
    /* 强制重新校准 */
    tp_dev.touchtype = 0;
    tp_dev.touchtype |= lcddev.dir & 0X01;
    tp_adjust();
    tp_save_adjust_data();
    
    /* 恢复标准界面 */
    draw_standard_ui("Touch: Calibrated");
  }
  else
  {
    uint8_t tp_init_result = tp_dev.init();
    if (tp_init_result == 0)
    {
      lcd_show_string(10, 90, 300, 32, 12, "Touch: Ready", BLUE);
      
      /* 绘制界面元素 */
      lcd_draw_rectangle(5, 120, 315, 470, BLACK);
      lcd_show_string(10, 130, 300, 32, 12, "Touch to draw", BLACK);
      lcd_show_string(10, 150, 300, 32, 12, "KEY0: Re-calibrate", BLACK);
    }
    else
    {
      lcd_show_string(10, 90, 300, 32, 12, "Touch: Error", RED);
    }
  }
  
  /* 初始化SD卡 */
  lcd_show_string(10, 110, 300, 32, 12, "Initializing SD card...", BLUE);
  HAL_Delay(100);  /* 给LCD一点时间显示 */
  
  uint8_t sd_init_result = sd_init();
  if (sd_init_result == 0)
  {
    lcd_show_string(10, 110, 300, 32, 12, "SD Card: OK        ", GREEN);
    show_sdcard_info();  /* 显示SD卡信息 */
    show_sd_debug_info(); /* 显示调试信息 */
    
    /* 添加按键说明 */
    lcd_show_string(10, 260, 300, 16, 12, "KEY0: Re-calibrate touch", BLACK);
    lcd_show_string(10, 280, 300, 16, 12, "KEY1: Test SD read sector 0", BLACK);
  }
  else
  {
    char error_str[40];
    sprintf(error_str, "SD Card: Error (code:%d)", sd_init_result);
    lcd_show_string(10, 110, 300, 32, 12, error_str, RED);
    lcd_show_string(10, 130, 300, 32, 12, "Please check SD card", RED);
  }
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 检查KEY0是否按下，进行重新校准 */
    static uint8_t key0_pressed = 0;
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)  /* KEY0按下 */
    {
      if (!key0_pressed)
      {
        key0_pressed = 1;
        HAL_Delay(50);  /* 消抖 */
        
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)  /* 确认按下 */
        {
          /* 重新校准 */
          tp_adjust();
          tp_save_adjust_data();
          
          /* 恢复标准界面 */
          draw_standard_ui("Touch: Re-calibrated");
        }
      }
    }
    else
    {
      key0_pressed = 0;
    }
    
    /* 检查KEY1是否按下，测试SD卡读取 */
    static uint8_t key1_pressed = 0;
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)  /* KEY1按下 */
    {
      if (!key1_pressed)
      {
        key1_pressed = 1;
        HAL_Delay(50);  /* 消抖 */
        
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)  /* 确认按下 */
        {
          /* 测试SD卡读取 */
          sd_test_read(0, 1);  /* 读取第0扇区 */
        }
      }
    }
    else
    {
      key1_pressed = 0;
    }
    
    /* 触摸屏扫描 */
    tp_dev.scan(0);  /* 扫描触摸屏，使用屏幕坐标模式 */
    
    if (tp_dev.sta & TP_PRES_DOWN)  /* 触摸屏被按下 */
    {
      /* 检查坐标有效性 */
      if (tp_dev.x[0] < lcddev.width && tp_dev.y[0] < lcddev.height && 
          tp_dev.x[0] != 0xFFFF && tp_dev.y[0] != 0xFFFF)
      {
        /* 在触摸区域内画红点 */
        if (tp_dev.y[0] > 160)  /* 只在下方区域画点，避免覆盖文字 */
        {
          tp_draw_big_point(tp_dev.x[0], tp_dev.y[0], RED);
        }
        
        /* 显示当前坐标 */
        char coord_str[50];
        sprintf(coord_str, "X:%03d Y:%03d     ", tp_dev.x[0], tp_dev.y[0]);
        lcd_show_string(200, 90, 100, 32, 12, coord_str, BLUE);
      }
    }

    
    // HAL_Delay(10);  /* 延时10ms */
    // HAL_UART_Transmit(&huart1,(uint8_t *)"666", 3, 1);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* LED闪烁指示程序不运行  */
    // static uint32_t led_counter = 0;
    // led_counter++;
    //
    // if (led_counter % 5000 == 0)  /* 降低LED闪烁频率 */
    // {
    //   HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_5); // LED1 toggle
    // }
    // if (led_counter % 10000 == 0)  /* 降低LED闪烁频率 */
    // {
    //   HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5); // LED0 toggle
    // }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
