#ifndef LCD_TEXT_H
#define LCD_TEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "display_port.h"

/*
 * Simple 8x16 ASCII text renderer on top of display_port.
 * Characters are drawn with fg (foreground) and bg (background) colors;
 * use bg = DISPLAY_COLOR_BLACK to avoid repainting the background.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_TEXT_CHAR_WIDTH  8U
#define LCD_TEXT_CHAR_HEIGHT 16U

typedef enum
{
    LCD_TEXT_OK = 0,
    LCD_TEXT_ERROR_NOT_READY,
    LCD_TEXT_ERROR_INVALID_ARGUMENT,
    LCD_TEXT_ERROR_OUT_OF_RANGE
} LCD_TEXT_STATUS_T;

/* Draws one character with its top-left corner at (x, y). */
LCD_TEXT_STATUS_T lcd_text_draw_char(uint16_t x, uint16_t y, char ch,
                                     uint16_t fg, uint16_t bg);

/* Draws a NUL terminated string starting at (x, y). */
LCD_TEXT_STATUS_T lcd_text_draw_string(uint16_t x, uint16_t y, const char *str,
                                       uint16_t fg, uint16_t bg);

/* Marks the renderer ready/unready (called by display_port_init). */
void lcd_text_init(void);
void lcd_text_deinit(void);

/* Text width/height helpers for layout. */
uint16_t lcd_text_string_width(const char *str);
uint16_t lcd_text_string_height(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_TEXT_H */
