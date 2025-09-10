#include "fsmc_debug.h"
#include "fsmc.h"

// Test different FSMC Bank4 address mappings
#define FSMC_BANK4_BASE     0x6C000000UL

// Different possible address calculations for A16 line
#define ADDR_TEST1          (FSMC_BANK4_BASE)           // 0x6C000000
#define ADDR_TEST2          (FSMC_BANK4_BASE + 0x2)     // 0x6C000002  
#define ADDR_TEST3          (FSMC_BANK4_BASE + 0x10000) // 0x6C010000 (A15)
#define ADDR_TEST4          (FSMC_BANK4_BASE + 0x20000) // 0x6C020000 (A16)
#define ADDR_TEST5          (FSMC_BANK4_BASE + 0x40000) // 0x6C040000 (A17)

void fsmc_memory_test(void)
{
    volatile uint16_t *test_addr;
    uint16_t test_value = 0x1234;
    uint16_t read_value;
    
    // Initialize FSMC
    MX_FSMC_Init();
    HAL_Delay(100);
    
    // Test 1: Basic write/read to base address
    test_addr = (volatile uint16_t *)ADDR_TEST1;
    *test_addr = test_value;
    read_value = *test_addr;
    
    // Blink LED to indicate test progress
    for(int i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET);
        HAL_Delay(100);
    }
    
    // Test different addresses
    volatile uint16_t *addrs[] = {
        (volatile uint16_t *)ADDR_TEST1,
        (volatile uint16_t *)ADDR_TEST2,
        (volatile uint16_t *)ADDR_TEST3,
        (volatile uint16_t *)ADDR_TEST4,
        (volatile uint16_t *)ADDR_TEST5
    };
    
    for(int i = 0; i < 5; i++) {
        // Write test pattern
        *addrs[i] = 0x5555;
        HAL_Delay(10);
        *addrs[i] = 0xAAAA;
        HAL_Delay(10);
        *addrs[i] = 0xFFFF;
        HAL_Delay(10);
        *addrs[i] = 0x0000;
        HAL_Delay(100);
    }
}

void fsmc_debug_test(void)
{
    // Initialize FSMC
    MX_FSMC_Init();
    HAL_Delay(100);
    
    // Test sequence: Try to communicate with LCD using different address mappings
    
    // Method 1: Standard calculation for A16 (should be correct)
    volatile uint16_t *lcd_reg = (volatile uint16_t *)0x6C000000;
    volatile uint16_t *lcd_data = (volatile uint16_t *)0x6C020000;
    
    // Try LCD reset command
    *lcd_reg = 0x01;  // Software reset
    HAL_Delay(150);
    
    *lcd_reg = 0x11;  // Sleep out
    HAL_Delay(150);
    
    // Try to read LCD ID
    *lcd_reg = 0x04;  // Read display ID
    uint16_t id1 = *lcd_data;
    uint16_t id2 = *lcd_data;
    uint16_t id3 = *lcd_data;
    
    // Method 2: Try with different address offset
    lcd_data = (volatile uint16_t *)0x6C000002;
    *lcd_reg = 0x04;  // Read display ID
    HAL_Delay(10);
    
    // Method 3: Try A15 instead of A16
    lcd_data = (volatile uint16_t *)0x6C010000;
    *lcd_reg = 0x04;  // Read display ID
    HAL_Delay(10);
    
    // Method 4: Try the official formula calculation
    // Base = 0x60000000 + (0x4000000 * 3) | ((1 << 16) * 2 - 2)
    uint32_t calculated_base = 0x60000000 + (0x4000000 * 3);
    calculated_base |= ((1UL << 16) * 2 - 2);
    
    volatile uint16_t *calc_reg = (volatile uint16_t *)calculated_base;
    volatile uint16_t *calc_data = (volatile uint16_t *)(calculated_base + 2);
    
    *calc_reg = 0x01;  // Software reset
    HAL_Delay(150);
    
    // Try simple display commands with each method
    uint32_t test_addresses[][2] = {
        {0x6C000000, 0x6C020000},  // A16 method
        {0x6C000000, 0x6C000002},  // +2 method  
        {0x6C000000, 0x6C010000},  // A15 method
        {calculated_base, calculated_base + 2}  // Formula method
    };
    
    for(int method = 0; method < 4; method++) {
        volatile uint16_t *reg = (volatile uint16_t *)test_addresses[method][0];
        volatile uint16_t *data = (volatile uint16_t *)test_addresses[method][1];
        
        // Basic LCD initialization
        *reg = 0x01; HAL_Delay(120);  // Reset
        *reg = 0x11; HAL_Delay(120);  // Sleep out
        *reg = 0x29; HAL_Delay(50);   // Display on
        
        // Set a small area to red
        *reg = 0x2A;  // Column address
        *data = 0x00; *data = 0x00; *data = 0x00; *data = 0x32;
        
        *reg = 0x2B;  // Row address  
        *data = 0x00; *data = 0x00; *data = 0x00; *data = 0x32;
        
        *reg = 0x2C;  // Memory write
        
        // Fill small area with different colors for each method
        uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF}; // Red, Green, Blue, White
        
        for(int i = 0; i < 1000; i++) {
            *data = colors[method];
        }
        
        HAL_Delay(500);  // Wait between methods
    }
} 