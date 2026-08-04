#ifndef BSP_PRINTF_H
#define BSP_PRINTF_H

#include <stddef.h>
#include <stdint.h>

void bsp_debug_uart_init(uint32_t baud_rate);
size_t bsp_debug_uart_write(const uint8_t *data, size_t length);

#endif
