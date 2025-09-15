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
#include "fatfs.h"
#include "sdio.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "nt35310_alientek.h"
#include "hr2046.h"
#include "vs1053_port.h"
#include "lcdfont.h"
#include "sdio_sdcard.h"
#include "audio_player.h"


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
// 音频播放器按键处理函数声明
void audio_handle_key1_play(void);      /* KEY1: 播放/暂停 */
void audio_handle_key2_next(void);      /* KEY2: 下一首 */
void audio_handle_key0_prev(void);      /* KEY0: 上一首 */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


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
  MX_USART1_UART_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  /* 初始化LCD - 使用正点原子的方式 */
  lcd_init();
  
  /* 清理LCD显示区域，设置音乐播放器界面 */
  lcd_fill(0, 0, 320, 480, WHITE);  /* 清屏 */
  
  /* 显示音乐播放器标题 */
  lcd_show_string(10, 30, 300, 24, 16, "STM32 Music Player", BLACK);
  lcd_show_string(10, 60, 300, 16, 12, "KEY0:Prev | KEY1:Play | KEY2:Next", BLUE);
  
  /* 清理调试区域和音乐信息区域 */
  lcd_fill(10, 50, 310, 120, WHITE);   /* 清理上方调试区 */
  lcd_fill(10, 140, 310, 300, WHITE);  /* 清理中间调试区 */
  lcd_fill(10, 320, 310, 400, WHITE);  /* 清理音乐信息区 */
  
  /*初始化所有外设 都放这*/

  uint8_t init_result = device_init_all();
  if (init_result > 0)
  {
    /* 有设备初始化失败*/
    char status_str[50];
    sprintf(status_str, "Warning: %d device(s) failed", init_result);
    lcd_show_string(10, 450, 300, 16, 12, status_str, RED);
  }

  /*debug info*/
  // sd_show_complete_info();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 主循环运行指示器 */
    static uint32_t loop_counter = 0;
    loop_counter++;
    if (loop_counter % 200000 == 0) {  /* 每20万次循环更新一次状态 */
        char status_str[40];
        sprintf(status_str, "Music Player Ready [%lu]", loop_counter / 200000);
        lcd_show_string(10, 10, 300, 16, 12, status_str, BLUE);
    }

    /* 音频播放器按键控制 */
    audio_handle_key0_prev();         /* KEY0: 上一首 */
    audio_handle_key1_play();         /* KEY1: 播放/暂停 */
    audio_handle_key2_next();         /* KEY2: 下一首 */
    
    /* 音频播放任务 */
    audio_player_task();
    
    /* 处理触摸屏输入 */
    tp_handle_main_loop();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

/**
 * @brief KEY1按键处理 - 播放/暂停控制
 */
void audio_handle_key1_play(void)
{
    static uint8_t key1_pressed = 0;
    
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)  /* KEY1按下 */
    {
        if (!key1_pressed)
        {
            key1_pressed = 1;
            HAL_Delay(50);  /* 消抖 */
            
            if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)  /* 确认按下 */
            {
                /* 播放当前歌曲 */
                audio_player_test_play();
            }
        }
    }
    else
    {
        key1_pressed = 0;
    }
}

/**
 * @brief KEY2按键处理 - 下一首
 */
void audio_handle_key2_next(void)
{
    static uint8_t key2_pressed = 0;
    
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET)  /* KEY2按下 */
    {
        if (!key2_pressed)
        {
            key2_pressed = 1;
            HAL_Delay(50);  /* 消抖 */
            
            if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET)  /* 确认按下 */
            {
                if (audio_player_next())
                {
                    /* 清理显示区域 */
                    lcd_fill(10, 320, 310, 400, WHITE);
                    
                    /* 显示当前文件名 */
                    const char* filename = audio_player_get_current_file();
                    if (filename && strlen(filename) > 0)
                    {
                        char display_name[50];
                        snprintf(display_name, sizeof(display_name), "Next: %.35s", filename);
                        lcd_show_string(10, 320, 300, 16, 12, display_name, BLACK);
                    }
                }
                else
                {
                    lcd_show_string(10, 320, 300, 16, 12, "No next song available", RED);
                }
            }
        }
    }
    else
    {
        key2_pressed = 0;
    }
}

/**
 * @brief KEY0按键处理 - 上一首
 */
void audio_handle_key0_prev(void)
{
    static uint8_t key0_pressed = 0;
    
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)  /* KEY0按下 */
    {
        if (!key0_pressed)
        {
            key0_pressed = 1;
            HAL_Delay(50);  /* 消抖 */
            
            if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)  /* 确认按下 */
            {
                if (audio_player_prev())
                {
                    /* 清理显示区域 */
                    lcd_fill(10, 320, 310, 400, WHITE);
                    
                    /* 显示当前文件名 */
                    const char* filename = audio_player_get_current_file();
                    if (filename && strlen(filename) > 0)
                    {
                        char display_name[50];
                        snprintf(display_name, sizeof(display_name), "Prev: %.35s", filename);
                        lcd_show_string(10, 320, 300, 16, 12, display_name, BLACK);
                    }
                }
                else
                {
                    lcd_show_string(10, 320, 300, 16, 12, "No previous song available", RED);
                }
            }
        }
    }
    else
    {
        key0_pressed = 0;
    }
}

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
