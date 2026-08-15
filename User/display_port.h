#ifndef DISPLAY_PORT_H
#define DISPLAY_PORT_H

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_PORT_WIDTH  240U
#define DISPLAY_PORT_HEIGHT 240U

#define DISPLAY_COLOR_BLACK 0x0000U
#define DISPLAY_COLOR_WHITE 0xFFFFU
#define DISPLAY_COLOR_RED   0xF800U
#define DISPLAY_COLOR_GREEN 0x07E0U
#define DISPLAY_COLOR_BLUE  0x001FU
#define DISPLAY_COLOR_YELLOW 0xFFE0U
#define DISPLAY_COLOR_CYAN  0x07FFU

typedef struct
{
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} DISPLAY_AREA_T;

typedef enum
{
    DISPLAY_PORT_OK = 0,
    DISPLAY_PORT_ERROR_INVALID_ARGUMENT,
    DISPLAY_PORT_ERROR_NOT_INITIALIZED,
    DISPLAY_PORT_ERROR_DRIVER
} DISPLAY_PORT_STATUS_T;

typedef void (*DISPLAY_PORT_FLUSH_COMPLETE_T)(void *context);

DISPLAY_PORT_STATUS_T display_port_init(void);
bool display_port_is_ready(void);
DISPLAY_PORT_STATUS_T display_port_flush(
    const DISPLAY_AREA_T *area,
    const uint16_t *pixels,
    DISPLAY_PORT_FLUSH_COMPLETE_T complete,
    void *context);
DISPLAY_PORT_STATUS_T display_port_fill(const DISPLAY_AREA_T *area,
                                        uint16_t color);

#endif
