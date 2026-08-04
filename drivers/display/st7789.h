#ifndef ST7789_H
#define ST7789_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    ST7789_OK = 0,
    ST7789_ERROR_INVALID_ARGUMENT,
    ST7789_ERROR_BUS,
    ST7789_ERROR_NOT_INITIALIZED,
    ST7789_ERROR_OUT_OF_RANGE
} ST7789_STATUS_T;

typedef enum
{
    ST7789_ROTATION_0 = 0,
    ST7789_ROTATION_90,
    ST7789_ROTATION_180,
    ST7789_ROTATION_270
} ST7789_ROTATION_T;

typedef struct
{
    bool (*write)(const uint8_t *data, size_t size);
    void (*set_data_mode)(bool data_mode);
    void (*set_reset)(bool high);
    void (*delay_ms)(uint32_t delay_ms);
} ST7789_BUS_T;

typedef struct
{
    ST7789_BUS_T bus;
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    ST7789_ROTATION_T rotation;
    bool initialized;
} ST7789_T;

ST7789_STATUS_T st7789_init(ST7789_T *display,
                            const ST7789_BUS_T *bus,
                            ST7789_ROTATION_T rotation);
ST7789_STATUS_T st7789_set_window(ST7789_T *display,
                                  uint16_t x1,
                                  uint16_t y1,
                                  uint16_t x2,
                                  uint16_t y2);
ST7789_STATUS_T st7789_write_pixels(ST7789_T *display,
                                    const uint16_t *pixels,
                                    size_t pixel_count);
ST7789_STATUS_T st7789_fill(ST7789_T *display,
                            uint16_t color,
                            size_t pixel_count);

#endif
