#include "bsp_led.h"

#include <stdint.h>

#include "board.h"

static const uint16_t s_led_pins[BSP_LED_COUNT] =
{
    [BSP_LED_USB] = BOARD_LED_USB_PIN,
    [BSP_LED_SWD] = BOARD_LED_SWD_PIN,
    [BSP_LED_CDC] = BOARD_LED_CDC_PIN,
};

static bool bsp_led_is_valid(BSP_LED_T led)
{
    return (unsigned int)led < (unsigned int)BSP_LED_COUNT;
}

void bsp_led_init(void)
{
    GPIO_Config_T config;
    const uint16_t all_leds = BOARD_LED_USB_PIN |
                              BOARD_LED_SWD_PIN |
                              BOARD_LED_CDC_PIN;

    RCM_EnableAHB1PeriphClock(BOARD_LED_GPIO_CLOCK);
    GPIO_ResetBit(GPIOE, all_leds);

    GPIO_ConfigStructInit(&config);
    config.pin = all_leds;
    config.mode = GPIO_MODE_OUT;
    config.speed = GPIO_SPEED_2MHz;
    config.otype = GPIO_OTYPE_PP;
    config.pupd = GPIO_PUPD_NOPULL;
    GPIO_Config(GPIOE, &config);
}

void bsp_led_set(BSP_LED_T led, bool on)
{
    if (!bsp_led_is_valid(led))
    {
        return;
    }

    if (on)
    {
        GPIO_SetBit(GPIOE, s_led_pins[led]);
    }
    else
    {
        GPIO_ResetBit(GPIOE, s_led_pins[led]);
    }
}

void bsp_led_toggle(BSP_LED_T led)
{
    if (bsp_led_is_valid(led))
    {
        GPIO_ToggleBit(GPIOE, s_led_pins[led]);
    }
}

bool bsp_led_is_on(BSP_LED_T led)
{
    if (!bsp_led_is_valid(led))
    {
        return false;
    }

    return GPIO_ReadOutputBit(GPIOE, s_led_pins[led]) != 0U;
}
