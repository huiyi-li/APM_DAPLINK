#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdbool.h>

typedef enum
{
    BSP_LED_USB = 0,
    BSP_LED_SWD,
    BSP_LED_CDC,
    BSP_LED_COUNT
} BSP_LED_T;

void bsp_led_init(void);
void bsp_led_set(BSP_LED_T led, bool on);
void bsp_led_toggle(BSP_LED_T led);
bool bsp_led_is_on(BSP_LED_T led);

#endif
