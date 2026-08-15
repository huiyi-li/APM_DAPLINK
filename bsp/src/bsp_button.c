#include "bsp_button.h"

#include <stdio.h>
#include "bsp_printf.h"
#include <string.h>

#include "board.h"

/*
 * Board level button implementation built on the portable BUTTON_T
 * framework. Pins are taken from board.h; every button event is also
 * logged to the debug UART for debugging.
 */

static BUTTON_T s_buttons[BSP_BUTTON_COUNT];
static BSP_BUTTON_EVENT_CB s_app_event_cb;
static void *s_app_event_user_data;

/* Board buttons are floating by default and read HIGH when pressed,
 * so use internal pull-downs and active-high polarity. */
static const BUTTON_CONFIG_T s_board_button_config = {
    .debounce_ms = 30U,
    .long_press_ms = 800U,
    .extra_long_press_ms = 3000U,
    .multi_click_window_ms = 350U,
    .active_high = true,
};

static const uint16_t s_button_pins[BSP_BUTTON_COUNT] = {
    [BSP_BUTTON_0] = BOARD_BUTTON_0_PIN,
    [BSP_BUTTON_1] = BOARD_BUTTON_1_PIN,
    [BSP_BUTTON_2] = BOARD_BUTTON_2_PIN,
};

static bool bsp_button_read_level(void *context)
{
    const BSP_BUTTON_T button = (BSP_BUTTON_T)(uintptr_t)context;

    /* Physical level: true = high. Active-low polarity is handled by
     * the framework through config.active_high. */
    return GPIO_ReadInputBit(BOARD_BUTTON_PORT, s_button_pins[button]) == BIT_SET;
}

static const char *event_name(BUTTON_EVENT_T event)
{
    static const char *const names[BUTTON_EVENT_COUNT] = {
        [BUTTON_EVENT_PRESSED] = "PRESSED",
        [BUTTON_EVENT_RELEASED] = "RELEASED",
        [BUTTON_EVENT_CLICKED] = "CLICKED",
        [BUTTON_EVENT_DOUBLE_CLICKED] = "DOUBLE_CLICK",
        [BUTTON_EVENT_TRIPLE_CLICKED] = "TRIPLE_CLICK",
        [BUTTON_EVENT_MULTI_CLICKED] = "MULTI_CLICK",
        [BUTTON_EVENT_LONG_PRESSED] = "LONG_PRESS",
        [BUTTON_EVENT_EXTRA_LONG_PRESSED] = "EXTRA_LONG_PRESS",
    };

    if ((unsigned int)event < (unsigned int)BUTTON_EVENT_COUNT)
    {
        return names[event];
    }
    return "?";
}

static void bsp_button_event_cb(void *button,
                                BUTTON_EVENT_T event,
                                uint32_t param,
                                uint32_t now_ms,
                                void *user_data)
{
    const BUTTON_T *btn = (const BUTTON_T *)button;
    const uintptr_t index = (uintptr_t)btn->user_data;

    (void)now_ms;
    (void)user_data;

    if ((event == BUTTON_EVENT_MULTI_CLICKED) || (event == BUTTON_EVENT_CLICKED) ||
        (event == BUTTON_EVENT_DOUBLE_CLICKED) || (event == BUTTON_EVENT_TRIPLE_CLICKED))
    {
        printf("[BTN] K%lu %s x%lu\r\n",
               (unsigned long)index,
               event_name(event),
               (unsigned long)param);
    }
    else
    {
        printf("[BTN] K%lu %s\r\n", (unsigned long)index, event_name(event));
    }

    if (s_app_event_cb != NULL)
    {
        s_app_event_cb((BSP_BUTTON_T)index, event, param, now_ms, s_app_event_user_data);
    }
}

void bsp_button_init(void)
{
    GPIO_Config_T gpio_config;

    RCM_EnableAHB1PeriphClock(BOARD_BUTTON_GPIO_CLOCK);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_BUTTON_0_PIN | BOARD_BUTTON_1_PIN | BOARD_BUTTON_2_PIN;
    gpio_config.mode = GPIO_MODE_IN;
    gpio_config.speed = GPIO_SPEED_2MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_DOWN;
    GPIO_Config(BOARD_BUTTON_PORT, &gpio_config);

    for (unsigned int i = 0U; i < (unsigned int)BSP_BUTTON_COUNT; ++i)
    {
        const BUTTON_IO_T io = {
            .read_level = bsp_button_read_level,
            .context = (void *)(uintptr_t)i,
        };
        button_init(&s_buttons[i], &s_board_button_config, &io, bsp_button_event_cb, (void *)(uintptr_t)i);
    }
}

void bsp_button_process(uint32_t now_ms)
{
    for (unsigned int i = 0U; i < (unsigned int)BSP_BUTTON_COUNT; ++i)
    {
        button_process(&s_buttons[i], now_ms);
    }
}

void bsp_button_set_event_cb(BSP_BUTTON_EVENT_CB cb, void *user_data)
{
    s_app_event_cb = cb;
    s_app_event_user_data = user_data;
}

bool bsp_button_is_pressed(BSP_BUTTON_T button)
{
    if ((unsigned int)button >= (unsigned int)BSP_BUTTON_COUNT)
    {
        return false;
    }
    return button_is_active(&s_buttons[button]);
}

uint32_t bsp_button_press_duration_ms(BSP_BUTTON_T button, uint32_t now_ms)
{
    if ((unsigned int)button >= (unsigned int)BSP_BUTTON_COUNT)
    {
        return 0U;
    }
    return button_active_duration_ms(&s_buttons[button], now_ms);
}

BUTTON_T *bsp_button_get(BSP_BUTTON_T button)
{
    if ((unsigned int)button >= (unsigned int)BSP_BUTTON_COUNT)
    {
        return NULL;
    }
    return &s_buttons[button];
}
