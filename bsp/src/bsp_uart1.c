#include "bsp_uart1.h"
#include "chry_ringbuffer.h"
#include "apm32f4xx_misc.h"
extern chry_ringbuffer_t g_uartrx;
static void Usart1PinInit(void)
{
    GPIO_Config_T GPIO_configStruct;
    GPIO_ConfigStructInit(&GPIO_configStruct);

    /* Enable GPIO clock */
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOA);
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOD);

    RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_USART2);

    /* Connect PXx to USARTx_Tx */
    GPIO_ConfigPinAF(GPIOA, GPIO_PIN_SOURCE_2, GPIO_AF_USART2);

    /* Connect PXx to USARTx_Rx */
    GPIO_ConfigPinAF(GPIOD, GPIO_PIN_SOURCE_6, GPIO_AF_USART2);

    /* Configure USART Tx as alternate function push-pull */
    GPIO_configStruct.mode = GPIO_MODE_AF;
    GPIO_configStruct.pin = GPIO_PIN_2;
    GPIO_configStruct.speed = GPIO_SPEED_50MHz;
    GPIO_Config(GPIOA, &GPIO_configStruct);

    /* Configure USART Rx as input floating */
    GPIO_configStruct.mode = GPIO_MODE_AF;
    GPIO_configStruct.pin = GPIO_PIN_6;
    GPIO_Config(GPIOD, &GPIO_configStruct);
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
    USART_Config(USART2, &usartConfigStruct);

    /* Enable USART */
    USART_Enable(USART2);
}

void Usart1Config(uint32_t baud, uint8_t parity, uint8_t data_bit, uint8_t stop_bit)
{
    USART_Config_T usartConfigStruct;

    /* USART1 configuration */
    usartConfigStruct.baudRate = baud;
    usartConfigStruct.hardwareFlow = USART_HARDWARE_FLOW_NONE;
    usartConfigStruct.mode = USART_MODE_TX_RX;
    usartConfigStruct.parity = USART_PARITY_NONE;
    usartConfigStruct.stopBits = USART_STOP_BIT_1;
    usartConfigStruct.wordLength = USART_WORD_LEN_8B;
    USART_Config(USART2, &usartConfigStruct);

    /* Enable USART */
    USART_Enable(USART2);
}

static void Usart1NVICInit(void)
{
    USART_EnableInterrupt(USART2, USART_INT_RXBNE);
    USART_ClearStatusFlag(USART2, USART_FLAG_RXBNE);
    NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_3);
    
    NVIC_EnableIRQRequest(USART2_IRQn,1,0);
}

void bsp_uart1_init(uint32_t baud)
{
    Usart1PinInit();
    Usart1Init(baud);
    Usart1NVICInit();
}

void bsp_uart1_send(uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        while(!(USART2->STS & USART_FLAG_TXBE));
        USART2->DATA_B.DATA = data[i];
    }
}

void bsp_uart1_send_byte(uint8_t data)
{
    USART2->DATA_B.DATA = data;
}

void USART2_IRQHandler(void)
{
    uint8_t data;
    if(USART2->STS & USART_FLAG_RXBNE)
    {
        USART_ClearStatusFlag(USART2, USART_FLAG_RXBNE);
        data = USART2->DATA_B.DATA;
        // chry_ringbuffer_write_byte(&g_uartrx,data);
    }
}
