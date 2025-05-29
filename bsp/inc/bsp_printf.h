#ifndef __BSP_PRINTF_H__
#define __BSP_PRINTF_H__

#include "apm32f4xx_rcm.h"
#include "apm32f4xx_fmc.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_usart.h"
#include <stdio.h>

#define DEBUG_PORT_CLK      RCM_AHB1_PERIPH_GPIOA
#define DEBUG_COM_CLK       RCM_APB2_PERIPH_USART1
#define DEBUG_PORT          GPIOA
#define DEBUG_TX_PIN        GPIO_PIN_9
#define DEBUG_RX_PIN        GPIO_PIN_10
#define DEBUG_TX_SOURCE     GPIO_PIN_SOURCE_9
#define DEBUG_RX_SOURCE     GPIO_PIN_SOURCE_10   
#define DEBUG_COM           USART1
#define DEBUG_AF            GPIO_AF_USART1

void bspInitUart(uint32_t baud);
void UsartWirte(USART_T* usart,uint8_t *dat, uint32_t count);
#endif