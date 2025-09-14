#include "vs1053_port.h"

void vs1053_port_init(void)
{
    /* SPI1已经在main.c中初始化，这里不需要重复初始化 */
    /* 设置初始状态 */
    VS_RST_L();     /* 复位状态 */
    VS_XCS_H();     /* 禁用命令接口 */
    VS_XDCS_H();    /* 禁用数据接口 */
    HAL_Delay(10);
}

void vs1053_port_speed_low(void)
{
    /* 低速模式：预分频器64，约1.125MHz (72MHz/64) */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    HAL_SPI_Init(&hspi1);
}

void vs1053_port_speed_high(void)
{
    /* 高速模式：预分频器8，约9MHz (72MHz/8) */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    HAL_SPI_Init(&hspi1);
}

uint8_t vs1053_port_rw(uint8_t data)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void vs1053_write_data(const uint8_t *buf, uint16_t len)
{
    while (!VS_DREQ_READ()) {}
    VS_XDCS_L();
    HAL_SPI_Transmit(&hspi1, (uint8_t*)buf, len, HAL_MAX_DELAY);
    VS_XDCS_H();
} 