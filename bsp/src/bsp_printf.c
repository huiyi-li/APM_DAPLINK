#include "bsp_printf.h"

#include <stdio.h>

#include "apm32f4xx_usart.h"
#include "board.h"

void bsp_debug_uart_init(uint32_t baud_rate)
{
    GPIO_Config_T gpio_config;
    USART_Config_T usart_config;

    RCM_EnableAHB1PeriphClock(BOARD_DEBUG_GPIO_CLOCK);
    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_USART1);
    GPIO_ConfigPinAF(BOARD_DEBUG_TX_PORT,
                     BOARD_DEBUG_TX_PIN_SOURCE,
                     GPIO_AF_USART1);
    GPIO_ConfigPinAF(BOARD_DEBUG_RX_PORT,
                     BOARD_DEBUG_RX_PIN_SOURCE,
                     GPIO_AF_USART1);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_DEBUG_TX_PIN | BOARD_DEBUG_RX_PIN;
    gpio_config.mode = GPIO_MODE_AF;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(GPIOA, &gpio_config);

    USART_ConfigStructInit(&usart_config);
    usart_config.baudRate = baud_rate;
    usart_config.hardwareFlow = USART_HARDWARE_FLOW_NONE;
    usart_config.mode = USART_MODE_TX_RX;
    usart_config.parity = USART_PARITY_NONE;
    usart_config.stopBits = USART_STOP_BIT_1;
    usart_config.wordLength = USART_WORD_LEN_8B;
    USART_Config(USART1, &usart_config);
    USART_Enable(USART1);
}

size_t bsp_debug_uart_write(const uint8_t *data, size_t length)
{
    size_t written = 0U;

    if (data == NULL)
    {
        return 0U;
    }

    while (written < length)
    {
        while (USART_ReadStatusFlag(USART1, USART_FLAG_TXBE) == RESET)
        {
        }
        USART_TxData(USART1, data[written++]);
    }
    return written;
}

/* AC6 (armclang) defines __GNUC__ and uses _write; the ARM library printf
 * path uses fputc. Define both so printf never falls back to the
 * semihosting (BKPT) implementation. */
int fputc(int character, FILE *stream)
{
    const uint8_t data = (uint8_t)character;
    (void)stream;
    (void)bsp_debug_uart_write(&data, 1U);
    return character;
}

#if defined(__GNUC__)
int _write(int file, char *data, int length)
{
    (void)file;
    if ((data == NULL) || (length < 0))
    {
        return -1;
    }
    return (int)bsp_debug_uart_write((const uint8_t *)data, (size_t)length);
}
#endif
