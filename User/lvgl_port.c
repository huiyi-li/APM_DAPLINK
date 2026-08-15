#include "lvgl_port.h"

#include <stdio.h>
#include "lvgl.h"
#include "tx_api.h"
#include "display_port.h"

/*
 * LVGL display adaptation for the ST7789 (240x240) through display_port.
 *
 * Memory budget: 32KB LVGL heap + 15KB partial render buffer
 * (240 x 32 x 2 bytes). LVGL redraws in partial chunks via the flush
 * callback, so no full framebuffer is needed.
 */

#define LVGL_DISP_WIDTH  DISPLAY_PORT_WIDTH
#define LVGL_DISP_HEIGHT DISPLAY_PORT_HEIGHT
#define LVGL_BUF_ROWS    16U

static lv_display_t *s_display;
static uint8_t s_render_buf[LVGL_DISP_WIDTH * LVGL_BUF_ROWS * 2U];
static bool s_ready;

static void lvgl_flush_cb(lv_display_t *display,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    const DISPLAY_AREA_T disp_area = {
        .x1 = (uint16_t)area->x1,
        .y1 = (uint16_t)area->y1,
        .x2 = (uint16_t)area->x2,
        .y2 = (uint16_t)area->y2,
    };

    (void)display_port_flush(&disp_area, (const uint16_t *)px_map, NULL, NULL);
    lv_display_flush_ready(display);
}

static uint32_t lvgl_tick_get_cb(void)
{
    return (uint32_t)tx_time_get();
}

bool lvgl_port_init(void)
{
    if (s_ready)
    {
        return true;
    }
    if (!display_port_is_ready())
    {
        return false;
    }

    printf("[LVGL] lv_init...\r\n");
    lv_init();
    printf("[LVGL] lv_init done, mem pool ok\r\n");
    lv_tick_set_cb(lvgl_tick_get_cb);

    printf("[LVGL] create display...\r\n");
    s_display = lv_display_create(LVGL_DISP_WIDTH, LVGL_DISP_HEIGHT);
    if (s_display == NULL)
    {
        printf("[LVGL] display create FAILED\r\n");
        return false;
    }
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);
    lv_display_set_buffers(s_display,
                           s_render_buf,
                           NULL,
                           sizeof(s_render_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    printf("[LVGL] display created\r\n");

    s_ready = true;
    return true;
}

void lvgl_port_handler(void)
{
    if (s_ready)
    {
        lv_timer_handler();
    }
}
