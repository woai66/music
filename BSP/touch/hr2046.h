#ifndef HR2046_H
#define HR2046_H

#include "main.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pins from CubeMX labels
#define TP_CS_H()      HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_SET)
#define TP_CS_L()      HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_RESET)
#define TP_SCK_H()     HAL_GPIO_WritePin(T_SCKPF9_GPIO_Port, T_SCKPF9_Pin, GPIO_PIN_SET)
#define TP_SCK_L()     HAL_GPIO_WritePin(T_SCKPF9_GPIO_Port, T_SCKPF9_Pin, GPIO_PIN_RESET)
#define TP_MOSI_H()    HAL_GPIO_WritePin(T_MOSI_GPIO_Port, T_MOSI_Pin, GPIO_PIN_SET)
#define TP_MOSI_L()    HAL_GPIO_WritePin(T_MOSI_GPIO_Port, T_MOSI_Pin, GPIO_PIN_RESET)
#define TP_MISO_READ() (HAL_GPIO_ReadPin(T_MISO_GPIO_Port, T_MISO_Pin) == GPIO_PIN_SET)
#define TP_PEN_READ()  (HAL_GPIO_ReadPin(T_PEN_GPIO_Port, T_PEN_Pin) == GPIO_PIN_RESET) // active low

void hr2046_init(void);
bool hr2046_read(uint16_t *x, uint16_t *y, uint16_t *z);

#ifdef __cplusplus
}
#endif

#endif // HR2046_H 