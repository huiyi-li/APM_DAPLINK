#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Portable, object oriented push-button driver.
 *
 * The framework is completely hardware independent: the application
 * provides a level-read callback (BUTTON_IO_T) so the same button
 * object works with any MCU/GPIO/port expander. Each button instance
 * owns its configuration and state machine, and reports events through
 * a single callback, making it easy to add new event types (click
 * counting, long press, extra long press, ...) without touching the
 * platform layer.
 *
 * Event flow:
 *   PRESSED/RELEASED        - debounced edge events
 *   LONG_PRESSED            - held >= long_press_ms (once per press)
 *   EXTRA_LONG_PRESSED      - held >= extra_long_press_ms (once per press)
 *   CLICKED/DOUBLE/TRIPLE/  - resolved after release when no further
 *   MULTI_CLICKED           - press happens inside multi_click_window_ms
 *
 * A press that ends in LONG/EXTRA_LONG press is not counted as a click.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BUTTON_EVENT_PRESSED = 0,      /* debounced press edge              */
    BUTTON_EVENT_RELEASED,         /* debounced release edge            */
    BUTTON_EVENT_CLICKED,          /* single click, param = 1           */
    BUTTON_EVENT_DOUBLE_CLICKED,   /* double click, param = 2           */
    BUTTON_EVENT_TRIPLE_CLICKED,   /* triple click, param = 3           */
    BUTTON_EVENT_MULTI_CLICKED,    /* >= 4 clicks, param = click count  */
    BUTTON_EVENT_LONG_PRESSED,     /* held >= long_press_ms             */
    BUTTON_EVENT_EXTRA_LONG_PRESSED, /* held >= extra_long_press_ms     */
    BUTTON_EVENT_COUNT
} BUTTON_EVENT_T;

/* Reads the raw physical pin level. Return true for a high level,
 * false for a low level. Whether that means "pressed" is decided by
 * the button's active_high configuration. */
typedef bool (*BUTTON_READ_FN)(void *context);

typedef struct
{
    BUTTON_READ_FN read_level;
    void *context;
} BUTTON_IO_T;

typedef struct
{
    uint32_t debounce_ms;           /* debounce time                     */
    uint32_t long_press_ms;         /* long press threshold              */
    uint32_t extra_long_press_ms;   /* extra long press threshold, 0 = disabled */
    uint32_t multi_click_window_ms; /* window to merge repeated clicks   */
    bool active_high;               /* true: high level = pressed        */
} BUTTON_CONFIG_T;

/* Event callback. button: the instance that produced the event,
 * param: click count for *_CLICKED events, 0 otherwise. */
typedef void (*BUTTON_EVENT_CB)(void *button,
                                BUTTON_EVENT_T event,
                                uint32_t param,
                                uint32_t now_ms,
                                void *user_data);

typedef struct BUTTON_S
{
    BUTTON_CONFIG_T config;
    BUTTON_IO_T io;
    BUTTON_EVENT_CB event_cb;
    void *user_data;

    /* internal state */
    bool raw_active;
    bool stable_active;
    uint32_t raw_changed_at;
    uint32_t active_since;
    bool long_sent;
    bool extra_long_sent;
    uint32_t click_count;
    uint32_t last_release_at;
} BUTTON_T;

/* Default configuration: 30ms debounce, 800ms long press,
 * 3000ms extra long press, 350ms multi click window, active low. */
extern const BUTTON_CONFIG_T g_button_default_config;

void button_init(BUTTON_T *button,
                 const BUTTON_CONFIG_T *config,
                 const BUTTON_IO_T *io,
                 BUTTON_EVENT_CB event_cb,
                 void *user_data);

/* Call periodically (e.g. every 10ms) with a monotonic millisecond
 * timestamp. */
void button_process(BUTTON_T *button, uint32_t now_ms);

/* Debounced state / press duration helpers. */
bool button_is_active(const BUTTON_T *button);
uint32_t button_active_duration_ms(const BUTTON_T *button, uint32_t now_ms);
uint32_t button_get_click_count(const BUTTON_T *button);
void button_reset(BUTTON_T *button);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
