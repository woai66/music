#include "nt35310_simple.h"
#include "fsmc.h"

// Try multiple address mappings to find the correct one
#define LCD_REG_BASE1    0x6C000000UL  // Base address
#define LCD_DATA_BASE1   0x6C020000UL  // A16 = 1 -> +0x20000 (2^16 * 2)

#define LCD_REG_BASE2    0x6C000000UL  // Base address  
#define LCD_DATA_BASE2   0x6C000002UL  // Simple +2 offset

#define LCD_REG_BASE3    0x6C000000UL  // Base address
#define LCD_DATA_BASE3   0x6C010000UL  // A15 = 1 -> +0x10000

void lcd_write_cmd(uint16_t cmd)
{
    // Try first mapping (A16 high)
    *((volatile uint16_t *)LCD_REG_BASE1) = cmd;
}

void lcd_write_data(uint16_t data)
{
    // Try first mapping (A16 high)
    *((volatile uint16_t *)LCD_DATA_BASE1) = data;
}

void lcd_simple_test(void)
{
    // Initialize FSMC
    MX_FSMC_Init();
    HAL_Delay(100);
    
    // Test 1: Try with A16 address mapping (0x20000 offset)
    // This should be correct for A16 -> RS line
    
    // Software reset
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x01;
    HAL_Delay(120);
    
    // Sleep out
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x11;
    HAL_Delay(120);
    
    // Set pixel format to RGB565
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x3A;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x55;
    
    // Display on
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x29;
    HAL_Delay(50);
    
    // Set address window to full screen
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x2A;  // Column address set
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x00;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x00;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x01;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0xDF;
    
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x2B;  // Row address set  
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x00;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x00;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x01;
    *((volatile uint16_t *)LCD_DATA_BASE1) = 0x3F;
    
    // Memory write command
    *((volatile uint16_t *)LCD_REG_BASE1) = 0x2C;
    
    // Fill with red color - just a small area first
    for (uint32_t i = 0; i < 10000; i++) {
        *((volatile uint16_t *)LCD_DATA_BASE1) = 0xF800;  // Red
    }
    
    HAL_Delay(1000);
    
    // Try Test 2: Different address mapping if first fails
    // Fill with green color using different mapping
    *((volatile uint16_t *)LCD_REG_BASE2) = 0x2C;
    for (uint32_t i = 0; i < 10000; i++) {
        *((volatile uint16_t *)LCD_DATA_BASE2) = 0x07E0;  // Green
    }
    
    HAL_Delay(1000);
    
    // Try Test 3: Third address mapping
    *((volatile uint16_t *)LCD_REG_BASE3) = 0x2C;
    for (uint32_t i = 0; i < 10000; i++) {
        *((volatile uint16_t *)LCD_DATA_BASE3) = 0x001F;  // Blue
    }
} 