#ifndef BSP_LCD_BUS_H
#define BSP_LCD_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SPI transport selection:
 *   1 = hardware SPI3 (PC10 SCK / PC12 MOSI, faster)
 *   0 = software bit-bang SPI (same pins, GPIO toggling)
 */
#ifndef BSP_LCD_BUS_USE_HW_SPI
#define BSP_LCD_BUS_USE_HW_SPI 1
#endif

typedef enum
{
    BSP_LCD_BUS_OK = 0,
    BSP_LCD_BUS_ERROR_INVALID_ARGUMENT,
    BSP_LCD_BUS_ERROR_NOT_INITIALIZED,
    BSP_LCD_BUS_ERROR_TIMEOUT,
    BSP_LCD_BUS_ERROR_DRIVER
} BSP_LCD_BUS_STATUS_T;

BSP_LCD_BUS_STATUS_T bsp_lcd_bus_init(void);
void bsp_lcd_bus_set_reset(bool high);
void bsp_lcd_bus_set_data_mode(bool data_mode);
BSP_LCD_BUS_STATUS_T bsp_lcd_bus_write(const uint8_t *data, size_t size);

#endif
