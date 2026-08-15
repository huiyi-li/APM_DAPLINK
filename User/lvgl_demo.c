#include "lvgl_demo.h"

#include <stdio.h>
#include "lvgl.h"
#include "lvgl_port.h"

/* Simple LVGL demo: a counter label, a button and an arc, driven by an
 * LVGL timer. Exercises text rendering, hit testing and partial redraw. */

static lv_obj_t *s_counter_label;
static lv_obj_t *s_arc;
static uint32_t s_counter;

static void demo_timer_cb(lv_timer_t *timer)
{
    char text[24];

    (void)timer;
    s_counter++;
    (void)snprintf(text, sizeof(text), "APM32 LVGL %lu", (unsigned long)s_counter);
    lv_label_set_text(s_counter_label, text);
    if (s_arc != NULL)
    {
        lv_arc_set_value(s_arc, (int32_t)(s_counter % 101U));
    }
}

static void button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
        printf("[LVGL] button clicked\r\n");
    }
}

void lvgl_demo_start(void)
{
    lv_obj_t *screen;
    lv_obj_t *label;
    lv_obj_t *button;
    lv_obj_t *btn_label;

    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x102030), 0);

    label = lv_label_create(screen);
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

    s_counter_label = lv_label_create(screen);
    lv_label_set_text(s_counter_label, "counter 0");
    lv_obj_set_style_text_color(s_counter_label, lv_color_hex(0x00FF88), 0);
    lv_obj_align(s_counter_label, LV_ALIGN_TOP_MID, 0, 32);

    button = lv_button_create(screen);
    lv_obj_set_size(button, 120, 40);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, NULL);
    btn_label = lv_label_create(button);
    lv_label_set_text(btn_label, "Click me");
    lv_obj_center(btn_label);

    s_arc = lv_arc_create(screen);
    lv_obj_set_size(s_arc, 120, 120);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_value(s_arc, 0);
    lv_obj_align(s_arc, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_create(demo_timer_cb, 500, NULL);

    {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        printf("[LVGL] mem total=%lu free=%lu used=%lu %%\r\n",
               (unsigned long)mon.total_size,
               (unsigned long)mon.free_size,
               (unsigned long)mon.used_pct);
    }
}
