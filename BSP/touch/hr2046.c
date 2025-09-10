#include "hr2046.h"

static void tp_delay(void)
{
    // short delay for bit-bang; ~1 us at 72MHz with empty loop is variable
    for (volatile int i = 0; i < 20; ++i) { __NOP(); }
}

static uint16_t tp_transfer(uint8_t cmd)
{
    uint16_t data = 0;
    // Send command (8 bits, MSB first)
    for (int i = 7; i >= 0; --i) {
        if (cmd & (1 << i)) TP_MOSI_H(); else TP_MOSI_L();
        TP_SCK_H(); tp_delay(); TP_SCK_L(); tp_delay();
    }
    // Read 12-bit result (first null bit then 12 bits)
    // Clock 16 cycles total to be safe
    for (int i = 0; i < 16; ++i) {
        TP_SCK_H(); tp_delay();
        data <<= 1;
        if (TP_MISO_READ()) data |= 1;
        TP_SCK_L(); tp_delay();
    }
    // Data is left-aligned (top 12 bits), shift to 12-bit
    data >>= 4;
    return data & 0x0FFF;
}

void hr2046_init(void)
{
    TP_CS_H();
    TP_SCK_L();
    TP_MOSI_L();
}

bool hr2046_read(uint16_t *x, uint16_t *y, uint16_t *z)
{
    if (!TP_PEN_READ()) {
        // no touch
        return false;
    }
    uint16_t xi = 0, yi = 0, zi = 0;
    TP_CS_L();
    tp_delay();
    // Commands: 0x90 (Y), 0xD0 (X), 0xB0 (Z1)
    // Read a few samples and average
    const int samples = 4;
    for (int i = 0; i < samples; ++i) {
        uint16_t yv = tp_transfer(0x90);
        uint16_t xv = tp_transfer(0xD0);
        uint16_t zv = tp_transfer(0xB0);
        xi += xv; yi += yv; zi += zv;
    }
    TP_CS_H();
    xi /= samples; yi /= samples; zi /= samples;
    if (x) *x = xi;
    if (y) *y = yi;
    if (z) *z = zi;
    return true;
} 