#include "button.h"

#include <stddef.h>

const BUTTON_CONFIG_T g_button_default_config = {
    .debounce_ms = 30U,
    .long_press_ms = 800U,
    .extra_long_press_ms = 3000U,
    .multi_click_window_ms = 350U,
    .active_high = false,
};

static bool time_elapsed(uint32_t now, uint32_t start, uint32_t duration)
{
    return (uint32_t)(now - start) >= duration;
}

static bool time_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static void fire_event(BUTTON_T *button, BUTTON_EVENT_T event, uint32_t param, uint32_t now_ms)
{
    if (button->event_cb != NULL)
    {
        button->event_cb(button, event, param, now_ms, button->user_data);
    }
}

void button_init(BUTTON_T *button,
                 const BUTTON_CONFIG_T *config,
                 const BUTTON_IO_T *io,
                 BUTTON_EVENT_CB event_cb,
                 void *user_data)
{
    if (button == NULL)
    {
        return;
    }

    button->config = (config != NULL) ? *config : g_button_default_config;
    if (io != NULL)
    {
        button->io = *io;
    }
    else
    {
        button->io.read_level = NULL;
        button->io.context = NULL;
    }
    button->event_cb = event_cb;
    button->user_data = user_data;

    button_reset(button);
}

void button_reset(BUTTON_T *button)
{
    if (button == NULL)
    {
        return;
    }

    button->raw_active = false;
    button->stable_active = false;
    button->raw_changed_at = 0U;
    button->active_since = 0U;
    button->long_sent = false;
    button->extra_long_sent = false;
    button->click_count = 0U;
    button->last_release_at = 0U;
}

static bool button_read_active(BUTTON_T *button)
{
    if (button->io.read_level == NULL)
    {
        return false;
    }
    return button->io.read_level(button->io.context) == button->config.active_high;
}

/* Settle a pending multi-click sequence when the window expires. */
static void button_settle_clicks(BUTTON_T *button, uint32_t now_ms)
{
    if ((button->click_count == 0U) ||
        !time_elapsed(now_ms, button->last_release_at, button->config.multi_click_window_ms))
    {
        return;
    }

    switch (button->click_count)
    {
        case 1U:
            fire_event(button, BUTTON_EVENT_CLICKED, 1U, now_ms);
            break;
        case 2U:
            fire_event(button, BUTTON_EVENT_DOUBLE_CLICKED, 2U, now_ms);
            break;
        case 3U:
            fire_event(button, BUTTON_EVENT_TRIPLE_CLICKED, 3U, now_ms);
            break;
        default:
            fire_event(button, BUTTON_EVENT_MULTI_CLICKED, button->click_count, now_ms);
            break;
    }
    button->click_count = 0U;
}

void button_process(BUTTON_T *button, uint32_t now_ms)
{
    bool raw_active;
    bool press_detected;
    bool release_detected;

    if (button == NULL)
    {
        return;
    }

    raw_active = button_read_active(button);
    if (raw_active != button->raw_active)
    {
        button->raw_active = raw_active;
        button->raw_changed_at = now_ms;
    }

    /* Debounce */
    press_detected = false;
    release_detected = false;
    if ((button->stable_active != button->raw_active) &&
        time_elapsed(now_ms, button->raw_changed_at, button->config.debounce_ms))
    {
        button->stable_active = button->raw_active;
        if (button->stable_active)
        {
            press_detected = true;
        }
        else
        {
            release_detected = true;
        }
    }

    if (press_detected)
    {
        /* A previous click sequence expired while the key was released:
         * settle it before starting a new one. */
        button_settle_clicks(button, now_ms);

        button->active_since = now_ms;
        button->long_sent = false;
        button->extra_long_sent = false;
        fire_event(button, BUTTON_EVENT_PRESSED, 0U, now_ms);
    }

    if (release_detected)
    {
        if (button->long_sent || button->extra_long_sent)
        {
            /* Long press is not counted as a click. */
            button->click_count = 0U;
        }
        else
        {
            button->click_count++;
        }
        button->last_release_at = now_ms;
        fire_event(button, BUTTON_EVENT_RELEASED, 0U, now_ms);
    }

    if (!button->stable_active)
    {
        /* Released: resolve multi-click once the window expires. */
        button_settle_clicks(button, now_ms);
        return;
    }

    /* Held: long press detection. */
    if (!button->long_sent &&
        time_elapsed(now_ms, button->active_since, button->config.long_press_ms))
    {
        button->long_sent = true;
        fire_event(button, BUTTON_EVENT_LONG_PRESSED, 0U, now_ms);
    }
    else if ((button->config.extra_long_press_ms != 0U) &&
             button->long_sent &&
             !button->extra_long_sent &&
             time_elapsed(now_ms, button->active_since, button->config.extra_long_press_ms))
    {
        button->extra_long_sent = true;
        fire_event(button, BUTTON_EVENT_EXTRA_LONG_PRESSED, 0U, now_ms);
    }
}

bool button_is_active(const BUTTON_T *button)
{
    if (button == NULL)
    {
        return false;
    }
    return button->stable_active;
}

uint32_t button_active_duration_ms(const BUTTON_T *button, uint32_t now_ms)
{
    if ((button == NULL) || !button->stable_active)
    {
        return 0U;
    }
    return (uint32_t)(now_ms - button->active_since);
}

uint32_t button_get_click_count(const BUTTON_T *button)
{
    if (button == NULL)
    {
        return 0U;
    }
    return button->click_count;
}
