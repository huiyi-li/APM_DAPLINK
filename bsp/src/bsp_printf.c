#include "bsp_printf.h"
#include <stdio.h>

#if defined (__CC_ARM) || defined (__ICCARM__) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))

/*!
* @brief       Redirect C Library function printf to serial port.
*              After Redirection, you can use printf function.
*
* @param       ch:  The characters that need to be send.
*
* @param       *f:  pointer to a FILE that can recording all information
*              needed to control a stream
*
* @retval      The characters that need to be send.
*
* @note
*/
int fputc(int ch, FILE* f)
{
    /* send a byte of data to the serial port */
    USART_TxData(DEBUG_COM, (uint8_t)ch);

    /* wait for the data to be send */
    while (USART_ReadStatusFlag(DEBUG_COM, USART_FLAG_TXBE) == RESET);

    return (ch);
}

#elif defined (__GNUC__)

/*!
* @brief       Redirect C Library function printf to serial port.
*              After Redirection, you can use printf function.
*
* @param       file:  Meaningless in this function.
*
* @param       *ptr:  Buffer pointer for data to be sent.
*
* @param       len:  Length of data to be sent.
*
* @retval      The characters that need to be send.
*
* @note
*/
int _write(int file, char* ptr, int len)
{
    /* reset warning */
    (void)file;
    /* send data to the serial port */
    UsartWirte(DEBUG_COM, (uint8_t*)ptr, len);

    return len;
}

int _sbrk(int incr)
{
    (void)incr;
    return 0;
}

int _fstat(int file)
{
    (void)file;
    return 0;
}

void _read(int file, char* ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
}

#else
#warning Not supported compiler type
#endif

static void Usart1PinInit(void)
{
    GPIO_Config_T GPIO_configStruct;
    GPIO_ConfigStructInit(&GPIO_configStruct);

    /* Enable GPIO clock */
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOA);

    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_USART1);

    /* Connect PXx to USARTx_Tx */
    GPIO_ConfigPinAF(DEBUG_PORT, DEBUG_TX_SOURCE, DEBUG_AF);

    /* Connect PXx to USARTx_Rx */
    GPIO_ConfigPinAF(DEBUG_PORT, DEBUG_RX_SOURCE, DEBUG_AF);

    /* Configure USART Tx as alternate function push-pull */
    GPIO_configStruct.mode = GPIO_MODE_AF;
    GPIO_configStruct.pin = DEBUG_TX_PIN;
    GPIO_configStruct.speed = GPIO_SPEED_50MHz;
    GPIO_Config(DEBUG_PORT, &GPIO_configStruct);

    /* Configure USART Rx as input floating */
    GPIO_configStruct.mode = GPIO_MODE_AF;
    GPIO_configStruct.pin = DEBUG_RX_PIN;
    GPIO_Config(DEBUG_PORT, &GPIO_configStruct);
}

static void Usart1Init(uint32_t baud)
{
    USART_Config_T usartConfigStruct;

    /* USART1 configuration */
    usartConfigStruct.baudRate = baud;
    usartConfigStruct.hardwareFlow = USART_HARDWARE_FLOW_NONE;
    usartConfigStruct.mode = USART_MODE_TX_RX;
    usartConfigStruct.parity = USART_PARITY_NONE;
    usartConfigStruct.stopBits = USART_STOP_BIT_1;
    usartConfigStruct.wordLength = USART_WORD_LEN_8B;
    USART_Config(DEBUG_COM, &usartConfigStruct);

    /* Enable USART */
    USART_Enable(DEBUG_COM);
}
/*!
 * @brief       USART1 Initialization
 *
 * @param       None
 *
 * @retval      None
 *
 */
void bspInitUart(uint32_t baud)
{
    Usart1PinInit();
    /* USART1 configuration */
    Usart1Init(baud);
}

/*!
 * @brief       Serial port tramsimt data
 *
 * @param       pointer to date that need to be sent
 *
 * @retval      None
 *
 */
void UsartWirte(USART_T* usart,uint8_t *dat, uint32_t count)
{
    while(count--)
    {
        while(USART_ReadStatusFlag(usart, USART_FLAG_TXBE) == RESET);
        USART_TxData(usart, *dat++);
    }
}

// void USART1_IRQn()
// {

// }
