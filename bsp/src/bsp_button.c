#include "bsp_button.h"

#include <stddef.h>

#include "board.h"

#define BSP_BUTTON_EVENT_QUEUE_SIZE 16U

typedef struct
{
    bool raw_pressed;
    bool stable_pressed;
    bool long_press_sent;
    uint32_t raw_changed_at;
    uint32_t pressed_at;
    uint32_t next_repeat_at;
} BSP_BUTTON_STATE_T;

const BSP_BUTTON_CONFIG_T g_bsp_button_default_config =
{
    .debounce_ms = 30U,
    .long_press_ms = 800U,
    .repeat_delay_ms = 400U,
    .repeat_interval_ms = 150U,
};

static const uint16_t s_button_pins[BSP_BUTTON_COUNT] =
{
    [BSP_BUTTON_0] = BOARD_BUTTON_0_PIN,
    [BSP_BUTTON_1] = BOARD_BUTTON_1_PIN,
    [BSP_BUTTON_2] = BOARD_BUTTON_2_PIN,
};

static BSP_BUTTON_CONFIG_T s_config;
static BSP_BUTTON_STATE_T s_button_states[BSP_BUTTON_COUNT];
static BSP_BUTTON_EVENT_T s_event_queue[BSP_BUTTON_EVENT_QUEUE_SIZE];
static uint8_t s_event_head;
static uint8_t s_event_tail;
static uint32_t s_dropped_events;

static bool time_elapsed(uint32_t now, uint32_t start, uint32_t duration)
{
    return (uint32_t)(now - start) >= duration;
}

static bool time_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static bool read_button(BSP_BUTTON_T button)
{
    return GPIO_ReadInputBit(GPIOE, s_button_pins[button]) == 0U;
}

static void queue_event(BSP_BUTTON_T button,
                        BSP_BUTTON_EVENT_TYPE_T type,
                        uint32_t timestamp_ms)
{
    const uint8_t next = (uint8_t)((s_event_head + 1U) % BSP_BUTTON_EVENT_QUEUE_SIZE);

    if (next == s_event_tail)
    {
        s_dropped_events++;
        return;
    }

    s_event_queue[s_event_head].button = button;
    s_event_queue[s_event_head].type = type;
    s_event_queue[s_event_head].timestamp_ms = timestamp_ms;
    s_event_head = next;
}

void bsp_button_init(const BSP_BUTTON_CONFIG_T *config)
{
    GPIO_Config_T gpio_config;
    const uint16_t all_buttons = BOARD_BUTTON_0_PIN |
                                 BOARD_BUTTON_1_PIN |
                                 BOARD_BUTTON_2_PIN;

    s_config = (config != NULL) ? *config : g_bsp_button_default_config;
    s_event_head = 0U;
    s_event_tail = 0U;
    s_dropped_events = 0U;

    RCM_EnableAHB1PeriphClock(BOARD_BUTTON_GPIO_CLOCK);
    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = all_buttons;
    gpio_config.mode = GPIO_MODE_IN;
    gpio_config.speed = GPIO_SPEED_2MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(GPIOE, &gpio_config);

    for (unsigned int i = 0U; i < (unsigned int)BSP_BUTTON_COUNT; ++i)
    {
        s_button_states[i].raw_pressed = false;
        s_button_states[i].stable_pressed = false;
        s_button_states[i].long_press_sent = false;
        s_button_states[i].raw_changed_at = 0U;
        s_button_states[i].pressed_at = 0U;
        s_button_states[i].next_repeat_at = 0U;
    }
}

void bsp_button_process(uint32_t now_ms)
{
    for (unsigned int i = 0U; i < (unsigned int)BSP_BUTTON_COUNT; ++i)
    {
        BSP_BUTTON_STATE_T *state = &s_button_states[i];
        const BSP_BUTTON_T button = (BSP_BUTTON_T)i;
        const bool pressed = read_button(button);

        if (pressed != state->raw_pressed)
        {
            state->raw_pressed = pressed;
            state->raw_changed_at = now_ms;
        }

        if ((state->stable_pressed != state->raw_pressed) &&
            time_elapsed(now_ms, state->raw_changed_at, s_config.debounce_ms))
        {
            state->stable_pressed = state->raw_pressed;

            if (state->stable_pressed)
            {
                state->pressed_at = now_ms;
                state->long_press_sent = false;
                queue_event(button, BSP_BUTTON_EVENT_PRESS, now_ms);
            }
            else
            {
                queue_event(button, BSP_BUTTON_EVENT_RELEASE, now_ms);
                if (!state->long_press_sent)
                {
                    queue_event(button, BSP_BUTTON_EVENT_CLICK, now_ms);
                }
                state->long_press_sent = false;
            }
        }

        if (!state->stable_pressed)
        {
            continue;
        }

        if (!state->long_press_sent &&
            time_elapsed(now_ms, state->pressed_at, s_config.long_press_ms))
        {
            state->long_press_sent = true;
            state->next_repeat_at = now_ms + s_config.repeat_delay_ms;
            queue_event(button, BSP_BUTTON_EVENT_LONG_PRESS, now_ms);
        }
        else if (state->long_press_sent &&
                 (s_config.repeat_interval_ms != 0U) &&
                 time_reached(now_ms, state->next_repeat_at))
        {
            state->next_repeat_at = now_ms + s_config.repeat_interval_ms;
            queue_event(button, BSP_BUTTON_EVENT_REPEAT, now_ms);
        }
    }
}

bool bsp_button_get_event(BSP_BUTTON_EVENT_T *event)
{
    if ((event == NULL) || (s_event_tail == s_event_head))
    {
        return false;
    }

    *event = s_event_queue[s_event_tail];
    s_event_tail = (uint8_t)((s_event_tail + 1U) % BSP_BUTTON_EVENT_QUEUE_SIZE);
    return true;
}

bool bsp_button_is_pressed(BSP_BUTTON_T button)
{
    if ((unsigned int)button >= (unsigned int)BSP_BUTTON_COUNT)
    {
        return false;
    }

    return s_button_states[button].stable_pressed;
}

void bsp_button_clear_events(void)
{
    s_event_tail = s_event_head;
}

uint32_t bsp_button_get_dropped_event_count(void)
{
    return s_dropped_events;
}
