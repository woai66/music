#ifndef NT35310_SIMPLE_H
#define NT35310_SIMPLE_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

// Direct FSMC address mapping for testing
// Bank4 base address: 0x6C000000
// A16 = 1 means +0x20000 offset for 16-bit access
#define FSMC_LCD_REG    (*((volatile uint16_t *)0x6C000000))
#define FSMC_LCD_DATA   (*((volatile uint16_t *)0x6C020000))

void lcd_simple_test(void);
void lcd_write_cmd(uint16_t cmd);
void lcd_write_data(uint16_t data);

#ifdef __cplusplus
}
#endif

#endif 