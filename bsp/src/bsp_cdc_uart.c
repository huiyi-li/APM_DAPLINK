#include "bsp_cdc_uart.h"
#include "chry_ringbuffer.h"
#include "apm32f4xx_misc.h"
#include "apm32f4xx_usart.h"
#include "board.h"

/* CDC1 usb2uart ring buffer, defined in User/dap_main.c */
extern chry_ringbuffer_t g_uartrx1;

#define CDC_UART_BUFFER_SIZE 1024U
#define CDC_UART_BUFFER_MASK (CDC_UART_BUFFER_SIZE - 1U)

const BSP_CDC_UART_CONFIG_T g_bsp_cdc_uart_default_config =
{
    .baud_rate = 115200U,
    .data_bits = 8U,
    .parity = BSP_CDC_UART_PARITY_NONE,
    .stop_bits = BSP_CDC_UART_STOP_BITS_1,
};

static uint8_t s_rx_buffer[CDC_UART_BUFFER_SIZE];
static uint8_t s_tx_buffer[CDC_UART_BUFFER_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile uint16_t s_tx_head;
static volatile uint16_t s_tx_tail;
static volatile uint32_t s_rx_overflow_count;
static volatile uint32_t s_error_count;

static uint16_t ring_next(uint16_t index)
{
    return (uint16_t)((index + 1U) & CDC_UART_BUFFER_MASK);
}

static size_t ring_used(uint16_t head, uint16_t tail)
{
    return (size_t)((head - tail) & CDC_UART_BUFFER_MASK);
}

static bool translate_config(const BSP_CDC_UART_CONFIG_T *config,
                             USART_Config_T *usart_config)
{
    if ((config == NULL) || (config->baud_rate == 0U))
    {
        return false;
    }

    USART_ConfigStructInit(usart_config);
    usart_config->baudRate = config->baud_rate;
    usart_config->hardwareFlow = USART_HARDWARE_FLOW_NONE;
    usart_config->mode = USART_MODE_TX_RX;

    switch (config->parity)
    {
        case BSP_CDC_UART_PARITY_NONE:
            if (config->data_bits != 8U)
            {
                return false;
            }
            usart_config->parity = USART_PARITY_NONE;
            usart_config->wordLength = USART_WORD_LEN_8B;
            break;

        case BSP_CDC_UART_PARITY_ODD:
        case BSP_CDC_UART_PARITY_EVEN:
            if ((config->data_bits != 7U) && (config->data_bits != 8U))
            {
                return false;
            }
            usart_config->parity = (config->parity == BSP_CDC_UART_PARITY_ODD) ?
                                   USART_PARITY_ODD : USART_PARITY_EVEN;
            usart_config->wordLength = (config->data_bits == 8U) ?
                                       USART_WORD_LEN_9B : USART_WORD_LEN_8B;
            break;

        default:
            return false;
    }

    switch (config->stop_bits)
    {
        case BSP_CDC_UART_STOP_BITS_1:
            usart_config->stopBits = USART_STOP_BIT_1;
            break;
        case BSP_CDC_UART_STOP_BITS_1_5:
            usart_config->stopBits = USART_STOP_BIT_1_5;
            break;
        case BSP_CDC_UART_STOP_BITS_2:
            usart_config->stopBits = USART_STOP_BIT_2;
            break;
        default:
            return false;
    }

    return true;
}

static void configure_pins(void)
{
    GPIO_Config_T gpio_config;

    RCM_EnableAHB1PeriphClock(BOARD_CDC_UART_GPIO_CLOCK);
    RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_USART3);
    GPIO_ConfigPinAF(BOARD_CDC_UART_TX_PORT,
                     BOARD_CDC_UART_TX_PIN_SOURCE,
                     GPIO_AF_USART3);
    GPIO_ConfigPinAF(BOARD_CDC_UART_RX_PORT,
                     BOARD_CDC_UART_RX_PIN_SOURCE,
                     GPIO_AF_USART3);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_CDC_UART_TX_PIN | BOARD_CDC_UART_RX_PIN;
    gpio_config.mode = GPIO_MODE_AF;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(GPIOB, &gpio_config);
}

bool bsp_cdc_uart_init(const BSP_CDC_UART_CONFIG_T *config)
{
    const BSP_CDC_UART_CONFIG_T *selected =
        (config != NULL) ? config : &g_bsp_cdc_uart_default_config;

    configure_pins();
    s_rx_overflow_count = 0U;
    s_error_count = 0U;
    NVIC_EnableIRQRequest(USART3_IRQn, 0U, 0U);
    return bsp_cdc_uart_configure(selected);
}

bool bsp_cdc_uart_configure(const BSP_CDC_UART_CONFIG_T *config)
{
    USART_Config_T usart_config;

    if (!translate_config(config, &usart_config))
    {
        return false;
    }

    USART_DisableInterrupt(USART3, USART_INT_RXBNE);
    USART_DisableInterrupt(USART3, USART_INT_TXBE);
    USART_DisableInterrupt(USART3, USART_INT_ERR);
    USART_Disable(USART3);

    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_tx_head = 0U;
    s_tx_tail = 0U;

    USART_Config(USART3, &usart_config);
    USART_Enable(USART3);
    USART_EnableInterrupt(USART3, USART_INT_RXBNE);
    USART_EnableInterrupt(USART3, USART_INT_ERR);
    return true;
}

size_t bsp_cdc_uart_write(const uint8_t *data, size_t length)
{
    size_t written = 0U;

    if (data == NULL)
    {
        return 0U;
    }

    while (written < length)
    {
        /* Blocking, interrupt-free transmit. The TXBE interrupt path
         * competes with the RXBNE interrupt (same USART vector, no
         * preemption between them): while the handler is busy pushing TX
         * bytes, the single-byte RX register overruns on the loopback path
         * and bytes are lost. Sending from the thread context leaves the
         * USART3 interrupt free to service RXBNE immediately. */
        while ((USART3->STS & USART_FLAG_TXBE) == 0U)
        {
            /* wait for transmit register empty */
        }
        USART_TxData(USART3, data[written]);
        written++;
    }
    return written;
}

size_t bsp_cdc_uart_read(uint8_t *data, size_t length)
{
    size_t read = 0U;

    if (data == NULL)
    {
        return 0U;
    }

    while ((read < length) && (s_rx_tail != s_rx_head))
    {
        data[read++] = s_rx_buffer[s_rx_tail];
        s_rx_tail = ring_next(s_rx_tail);
    }
    return read;
}

size_t bsp_cdc_uart_rx_available(void)
{
    return ring_used(s_rx_head, s_rx_tail);
}

size_t bsp_cdc_uart_tx_free(void)
{
    return (CDC_UART_BUFFER_SIZE - 1U) - ring_used(s_tx_head, s_tx_tail);
}

uint32_t bsp_cdc_uart_get_rx_overflow_count(void)
{
    return s_rx_overflow_count;
}

uint32_t bsp_cdc_uart_get_error_count(void)
{
    return s_error_count;
}

void USART3_IRQHandler(void)
{
    const uint32_t status = USART3->STS;

    if ((status & USART_FLAG_RXBNE) != 0U)
    {
        /* Drain the receive register in a loop so bytes received while
         * this handler was delayed are not lost (overrun). */
        do
        {
            const uint8_t data = (uint8_t)USART_RxData(USART3);
            const uint16_t next = ring_next(s_rx_head);

            if (next == s_rx_tail)
            {
                s_rx_overflow_count++;
            }
            else
            {
                s_rx_buffer[s_rx_head] = data;
                s_rx_head = next;
            }
            /* Bridge CDC1 (COM8) to USART3: forward the byte to the CDC1
             * usb2uart ring buffer so it is sent out on the CDC1 IN endpoint. */
            chry_ringbuffer_write_byte(&g_uartrx1, data);
        } while ((USART3->STS & USART_FLAG_RXBNE) != 0U);
    }

    if ((status & (USART_FLAG_OVRE | USART_FLAG_NE |
                   USART_FLAG_FE | USART_FLAG_PE)) != 0U)
    {
        if ((status & USART_FLAG_RXBNE) == 0U)
        {
            (void)USART_RxData(USART3);
        }
        s_error_count++;
    }

    if (USART_ReadIntFlag(USART3, USART_INT_TXBE) != RESET)
    {
        /* Transmit one byte per TXBE event. Transmitting several bytes in a
         * tight loop here would overrun the single-byte RX buffer on the
         * loopback path (TX mirrored into RX) because this handler cannot
         * drain RX while it is busy pushing TX. */
        if (s_tx_tail != s_tx_head)
        {
            USART_TxData(USART3, s_tx_buffer[s_tx_tail]);
            s_tx_tail = ring_next(s_tx_tail);
        }
        else
        {
            USART_DisableInterrupt(USART3, USART_INT_TXBE);
        }
    }
}
