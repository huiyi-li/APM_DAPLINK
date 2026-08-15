#include "lcd_text.h"

#include <string.h>

#include "lcd_font_8x16.h"

static bool s_ready;

static LCD_TEXT_STATUS_T draw_char_pixels(uint16_t x, uint16_t y,
                                          const uint8_t *glyph,
                                          uint16_t fg, uint16_t bg)
{
    DISPLAY_AREA_T area;
    uint16_t pixels[LCD_TEXT_CHAR_WIDTH * LCD_TEXT_CHAR_HEIGHT];

    if (!s_ready)
    {
        return LCD_TEXT_ERROR_NOT_READY;
    }
    if ((x + LCD_TEXT_CHAR_WIDTH > DISPLAY_PORT_WIDTH) ||
        (y + LCD_TEXT_CHAR_HEIGHT > DISPLAY_PORT_HEIGHT))
    {
        return LCD_TEXT_ERROR_OUT_OF_RANGE;
    }

    for (uint16_t row = 0U; row < LCD_TEXT_CHAR_HEIGHT; ++row)
    {
        const uint8_t line = glyph[row];
        for (uint16_t col = 0U; col < LCD_TEXT_CHAR_WIDTH; ++col)
        {
            const uint16_t index = row * LCD_TEXT_CHAR_WIDTH + col;
            /* Font data bit 0 is the leftmost column. */
            pixels[index] = ((line & (0x01U << col)) != 0U) ? fg : bg;
        }
    }

    area.x1 = x;
    area.y1 = y;
    area.x2 = (uint16_t)(x + LCD_TEXT_CHAR_WIDTH - 1U);
    area.y2 = (uint16_t)(y + LCD_TEXT_CHAR_HEIGHT - 1U);

    return (display_port_flush(&area, pixels, NULL, NULL) == DISPLAY_PORT_OK)
               ? LCD_TEXT_OK
               : LCD_TEXT_ERROR_INVALID_ARGUMENT;
}

LCD_TEXT_STATUS_T lcd_text_draw_char(uint16_t x, uint16_t y, char ch,
                                     uint16_t fg, uint16_t bg)
{
    uint8_t index;

    if ((ch < 0x20) || (ch > 0x7E))
    {
        ch = ' ';
    }
    index = (uint8_t)((uint8_t)ch - 0x20U);
    return draw_char_pixels(x, y, lcd_font_8x16[index], fg, bg);
}

LCD_TEXT_STATUS_T lcd_text_draw_string(uint16_t x, uint16_t y, const char *str,
                                       uint16_t fg, uint16_t bg)
{
    LCD_TEXT_STATUS_T status = LCD_TEXT_OK;

    if (str == NULL)
    {
        return LCD_TEXT_ERROR_INVALID_ARGUMENT;
    }
    if (!s_ready)
    {
        return LCD_TEXT_ERROR_NOT_READY;
    }

    while ((*str != '\0') && (status == LCD_TEXT_OK))
    {
        status = lcd_text_draw_char(x, y, *str, fg, bg);
        x = (uint16_t)(x + LCD_TEXT_CHAR_WIDTH);
        str++;
    }
    return status;
}

uint16_t lcd_text_string_width(const char *str)
{
    return (str != NULL) ? (uint16_t)(strlen(str) * LCD_TEXT_CHAR_WIDTH) : 0U;
}

uint16_t lcd_text_string_height(void)
{
    return LCD_TEXT_CHAR_HEIGHT;
}

/* Called by display_port_init after the panel is ready. */
void lcd_text_init(void)
{
    s_ready = true;
}

void lcd_text_deinit(void)
{
    s_ready = false;
}
