#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_BUTTON_0 = 0,
    BSP_BUTTON_1,
    BSP_BUTTON_2,
    BSP_BUTTON_COUNT
} BSP_BUTTON_T;

typedef enum
{
    BSP_BUTTON_EVENT_PRESS = 0,
    BSP_BUTTON_EVENT_RELEASE,
    BSP_BUTTON_EVENT_CLICK,
    BSP_BUTTON_EVENT_LONG_PRESS,
    BSP_BUTTON_EVENT_REPEAT
} BSP_BUTTON_EVENT_TYPE_T;

typedef struct
{
    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t repeat_delay_ms;
    uint16_t repeat_interval_ms;
} BSP_BUTTON_CONFIG_T;

typedef struct
{
    BSP_BUTTON_T button;
    BSP_BUTTON_EVENT_TYPE_T type;
    uint32_t timestamp_ms;
} BSP_BUTTON_EVENT_T;

extern const BSP_BUTTON_CONFIG_T g_bsp_button_default_config;

void bsp_button_init(const BSP_BUTTON_CONFIG_T *config);
void bsp_button_process(uint32_t now_ms);
bool bsp_button_get_event(BSP_BUTTON_EVENT_T *event);
bool bsp_button_is_pressed(BSP_BUTTON_T button);
void bsp_button_clear_events(void);
uint32_t bsp_button_get_dropped_event_count(void);

#endif
