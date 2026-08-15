#include "display_port.h"

#include <stddef.h>

#include "apm32f4xx.h"
#include "bsp_lcd_bus.h"
#include "st7789.h"
#include "lcd_text.h"

static ST7789_T s_display;
static bool s_ready;

static bool display_bus_write(const uint8_t *data, size_t size)
{
    return bsp_lcd_bus_write(data, size) == BSP_LCD_BUS_OK;
}

static void display_delay_ms(uint32_t delay_ms)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t cycles_per_ms = SystemCoreClock / 1000U;
    const uint32_t delay_cycles = cycles_per_ms * delay_ms;

    while ((uint32_t)(DWT->CYCCNT - start) < delay_cycles)
    {
    }
}

static DISPLAY_PORT_STATUS_T display_validate_area(const DISPLAY_AREA_T *area,
                                                   size_t *pixel_count)
{
    size_t width;
    size_t height;

    if ((area == NULL) || (pixel_count == NULL) ||
        (area->x1 > area->x2) || (area->y1 > area->y2) ||
        (area->x2 >= DISPLAY_PORT_WIDTH) ||
        (area->y2 >= DISPLAY_PORT_HEIGHT))
    {
        return DISPLAY_PORT_ERROR_INVALID_ARGUMENT;
    }

    width = (size_t)area->x2 - area->x1 + 1U;
    height = (size_t)area->y2 - area->y1 + 1U;
    *pixel_count = width * height;
    return DISPLAY_PORT_OK;
}

DISPLAY_PORT_STATUS_T display_port_init(void)
{
    static const DISPLAY_AREA_T full_screen = {
        0U, 0U, DISPLAY_PORT_WIDTH - 1U, DISPLAY_PORT_HEIGHT - 1U
    };
    const ST7789_BUS_T bus = {
        .write = display_bus_write,
        .set_data_mode = bsp_lcd_bus_set_data_mode,
        .set_reset = bsp_lcd_bus_set_reset,
        .delay_ms = display_delay_ms,
    };

    s_ready = false;
    if (bsp_lcd_bus_init() != BSP_LCD_BUS_OK)
    {
        return DISPLAY_PORT_ERROR_DRIVER;
    }
    if (st7789_init(&s_display, &bus, ST7789_ROTATION_0) != ST7789_OK)
    {
        return DISPLAY_PORT_ERROR_DRIVER;
    }
    s_ready = true;
    lcd_text_init();
    if (display_port_fill(&full_screen, DISPLAY_COLOR_BLACK) != DISPLAY_PORT_OK)
    {
        s_ready = false;
        return DISPLAY_PORT_ERROR_DRIVER;
    }
    return DISPLAY_PORT_OK;
}

bool display_port_is_ready(void)
{
    return s_ready;
}

DISPLAY_PORT_STATUS_T display_port_flush(
    const DISPLAY_AREA_T *area,
    const uint16_t *pixels,
    DISPLAY_PORT_FLUSH_COMPLETE_T complete,
    void *context)
{
    DISPLAY_PORT_STATUS_T status;
    size_t pixel_count;

    if (!s_ready)
    {
        return DISPLAY_PORT_ERROR_NOT_INITIALIZED;
    }
    if (pixels == NULL)
    {
        return DISPLAY_PORT_ERROR_INVALID_ARGUMENT;
    }
    status = display_validate_area(area, &pixel_count);
    if (status != DISPLAY_PORT_OK)
    {
        return status;
    }

    if ((st7789_set_window(&s_display,
                           area->x1, area->y1, area->x2, area->y2) != ST7789_OK) ||
        (st7789_write_pixels(&s_display, pixels, pixel_count) != ST7789_OK))
    {
        return DISPLAY_PORT_ERROR_DRIVER;
    }

    if (complete != NULL)
    {
        complete(context);
    }
    return DISPLAY_PORT_OK;
}

DISPLAY_PORT_STATUS_T display_port_fill(const DISPLAY_AREA_T *area,
                                        uint16_t color)
{
    DISPLAY_PORT_STATUS_T status;
    size_t pixel_count;

    if (!s_ready)
    {
        return DISPLAY_PORT_ERROR_NOT_INITIALIZED;
    }
    status = display_validate_area(area, &pixel_count);
    if (status != DISPLAY_PORT_OK)
    {
        return status;
    }
    if ((st7789_set_window(&s_display,
                           area->x1, area->y1, area->x2, area->y2) != ST7789_OK) ||
        (st7789_fill(&s_display, color, pixel_count) != ST7789_OK))
    {
        return DISPLAY_PORT_ERROR_DRIVER;
    }
    return DISPLAY_PORT_OK;
}
