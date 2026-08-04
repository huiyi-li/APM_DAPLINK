#ifndef BSP_CDC_UART_H
#define BSP_CDC_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BSP_CDC_UART_PARITY_NONE = 0,
    BSP_CDC_UART_PARITY_ODD,
    BSP_CDC_UART_PARITY_EVEN
} BSP_CDC_UART_PARITY_T;

typedef enum
{
    BSP_CDC_UART_STOP_BITS_1 = 0,
    BSP_CDC_UART_STOP_BITS_1_5,
    BSP_CDC_UART_STOP_BITS_2
} BSP_CDC_UART_STOP_BITS_T;

typedef struct
{
    uint32_t baud_rate;
    uint8_t data_bits;
    BSP_CDC_UART_PARITY_T parity;
    BSP_CDC_UART_STOP_BITS_T stop_bits;
} BSP_CDC_UART_CONFIG_T;

extern const BSP_CDC_UART_CONFIG_T g_bsp_cdc_uart_default_config;

bool bsp_cdc_uart_init(const BSP_CDC_UART_CONFIG_T *config);
bool bsp_cdc_uart_configure(const BSP_CDC_UART_CONFIG_T *config);
size_t bsp_cdc_uart_write(const uint8_t *data, size_t length);
size_t bsp_cdc_uart_read(uint8_t *data, size_t length);
size_t bsp_cdc_uart_rx_available(void);
size_t bsp_cdc_uart_tx_free(void);
uint32_t bsp_cdc_uart_get_rx_overflow_count(void);
uint32_t bsp_cdc_uart_get_error_count(void);

#endif
