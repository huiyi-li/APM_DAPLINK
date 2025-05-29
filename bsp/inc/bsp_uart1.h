#ifndef __BSP_UART1_H__
#define __BSP_UART1_H__

#include "apm32f4xx_rcm.h"
#include "apm32f4xx_fmc.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_usart.h"

void bsp_uart1_init(uint32_t baud);
void bsp_uaer1_send(uint8_t *data, uint32_t len);

#endif
