/*
 * MCU backend: 3 physical buttons -> LVGL keypad indev.
 *
 * BUTTON_0 = UP (LV_KEY_PREV), BUTTON_1 = DOWN (LV_KEY_NEXT),
 * BUTTON_2 = OK: short press LV_KEY_ENTER, long press LV_KEY_ESC (back).
 * The buttons are wired through bsp_button (PE3/PE4/PE5, active low).
 */

#include <string.h>

#include "lvgl.h"
#include "app_keys.h"
#include "bsp_button.h"
#include "tx_api.h"

#define KEYQ_SZ 16
#define LONG_PRESS_MS 800U

static uint32_t keyq[KEYQ_SZ];
static int qh, qc;

static lv_indev_t *key_indev;
static lv_group_t *cur_group;

static uint32_t ok_press_start;
static bool      ok_held;

void app_keys_set_group(lv_group_t *g)
{
    cur_group = g;
    if (key_indev)
    {
        lv_indev_set_group(key_indev, g);
    }
}

lv_group_t *app_keys_get_group(void)
{
    return cur_group;
}

static void app_keys_push(uint32_t lvkey)
{
    if (qc < KEYQ_SZ)
    {
        keyq[(qh + qc) % KEYQ_SZ] = lvkey;
        qc++;
    }
}

static uint32_t keyq_pop(void)
{
    uint32_t k = keyq[qh];
    qh = (qh + 1) % KEYQ_SZ;
    qc--;
    return k;
}

static void kb_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static uint32_t last_key = 0;
    static bool key_held = false;

    if (!key_held && qc > 0)
    {
        last_key = keyq_pop();
        key_held = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = last_key;
    }
    else if (key_held)
    {
        key_held = false;
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
    }
    data->continue_reading = false;
}

/* bsp_button event callback (runs in thread_0, button scan context) */
static void app_keys_button_cb(BSP_BUTTON_T button, BUTTON_EVENT_T event,
                               uint32_t param, uint32_t now_ms,
                               void *user_data)
{
    (void)param;
    (void)user_data;

    switch (button)
    {
    case BSP_BUTTON_0:                 /* UP */
        if (event == BUTTON_EVENT_PRESSED)
        {
            app_keys_push(APP_KEY_UP);
        }
        break;
    case BSP_BUTTON_1:                 /* DOWN */
        if (event == BUTTON_EVENT_PRESSED)
        {
            app_keys_push(APP_KEY_DOWN);
        }
        break;
    case BSP_BUTTON_2:                 /* OK */
        if (event == BUTTON_EVENT_PRESSED)
        {
            ok_press_start = now_ms;
            ok_held = true;
        }
        else if (event == BUTTON_EVENT_RELEASED)
        {
            ok_held = false;
            if ((now_ms - ok_press_start) < LONG_PRESS_MS)
            {
                app_keys_push(APP_KEY_ENTER);
            }
            else
            {
                app_keys_push(APP_KEY_BACK);
            }
        }
        break;
    default:
        break;
    }
}

void app_keys_init(void)
{
    key_indev = lv_indev_create();
    lv_indev_set_display(key_indev, lv_display_get_default());
    lv_indev_set_type(key_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(key_indev, kb_read_cb);
    lv_indev_set_mode(key_indev, LV_INDEV_MODE_TIMER);

    bsp_button_set_event_cb(app_keys_button_cb, NULL);
}
