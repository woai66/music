#include "vs1053_port.h"

void vs1053_port_init(void)
{
    MX_SPI2_Init();
    VS_RST_L();
    HAL_Delay(10);
    VS_XCS_H();
    VS_XDCS_H();
    VS_RST_H();
    HAL_Delay(10);
}

void vs1053_port_speed_low(void)
{
    // Reinit SPI2 with lower speed if needed (prescaler 32)
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    HAL_SPI_Init(&hspi2);
}

void vs1053_port_speed_high(void)
{
    // Increase SPI2 speed (prescaler 8)
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    HAL_SPI_Init(&hspi2);
}

uint8_t vs1053_port_rw(uint8_t data)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void vs1053_write_data(const uint8_t *buf, uint16_t len)
{
    while (!VS_DREQ_READ()) {}
    VS_XDCS_L();
    HAL_SPI_Transmit(&hspi2, (uint8_t*)buf, len, HAL_MAX_DELAY);
    VS_XDCS_H();
} 