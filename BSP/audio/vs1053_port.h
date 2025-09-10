#ifndef VS1053_PORT_H
#define VS1053_PORT_H

#include "main.h"
#include "spi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Control pins
#define VS_XCS_H()      HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_SET)
#define VS_XCS_L()      HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_RESET)
#define VS_XDCS_H()     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_SET)
#define VS_XDCS_L()     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET)
#define VS_RST_H()      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_SET)
#define VS_RST_L()      HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET)
#define VS_DREQ_READ()  (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)

extern SPI_HandleTypeDef hspi2; // SPI2 for VS1053

void vs1053_port_init(void);
void vs1053_port_speed_low(void);
void vs1053_port_speed_high(void);
uint8_t vs1053_port_rw(uint8_t data);
void vs1053_write_data(const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // VS1053_PORT_H 