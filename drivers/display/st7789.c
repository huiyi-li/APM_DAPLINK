#include "st7789.h"

#define ST7789_CMD_SLEEP_OUT     0x11U
#define ST7789_CMD_INVERSION_ON  0x21U
#define ST7789_CMD_DISPLAY_ON    0x29U
#define ST7789_CMD_COLUMN_ADDR   0x2AU
#define ST7789_CMD_ROW_ADDR      0x2BU
#define ST7789_CMD_MEMORY_WRITE  0x2CU
#define ST7789_CMD_MADCTL        0x36U
#define ST7789_CMD_PIXEL_FORMAT  0x3AU

#define ST7789_WIDTH             240U
#define ST7789_HEIGHT            240U
#define ST7789_PIXEL_CHUNK       1024U

typedef struct
{
    uint8_t command;
    uint8_t length;
    uint8_t data[14];
} ST7789_INIT_COMMAND_T;

static const ST7789_INIT_COMMAND_T s_init_commands[] = {
    { 0xB2U, 5U, { 0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U } },
    { 0x35U, 1U, { 0x00U } },
    { ST7789_CMD_MADCTL, 1U, { 0x00U } },
    { ST7789_CMD_PIXEL_FORMAT, 1U, { 0x05U } },
    { 0xB7U, 1U, { 0x35U } },
    { 0xBBU, 1U, { 0x2DU } },
    { 0xC0U, 1U, { 0x2CU } },
    { 0xC2U, 1U, { 0x01U } },
    { 0xC3U, 1U, { 0x15U } },
    { 0xC4U, 1U, { 0x20U } },
    { 0xC6U, 1U, { 0x0FU } },
    { 0xD0U, 2U, { 0xA4U, 0xA1U } },
    { 0xD6U, 1U, { 0xA1U } },
    { 0xE0U, 14U, { 0x70U, 0x05U, 0x0AU, 0x0BU, 0x0AU, 0x27U, 0x2FU,
                    0x44U, 0x47U, 0x37U, 0x14U, 0x14U, 0x29U, 0x2FU } },
    { 0xE1U, 14U, { 0x70U, 0x07U, 0x0CU, 0x08U, 0x08U, 0x04U, 0x2FU,
                    0x33U, 0x46U, 0x18U, 0x15U, 0x15U, 0x2BU, 0x2DU } },
};

static ST7789_STATUS_T write_command(ST7789_T *display,
                                     uint8_t command,
                                     const uint8_t *data,
                                     size_t size)
{
    display->bus.set_data_mode(false);
    if (!display->bus.write(&command, 1U))
    {
        return ST7789_ERROR_BUS;
    }
    if (size != 0U)
    {
        display->bus.set_data_mode(true);
        if (!display->bus.write(data, size))
        {
            return ST7789_ERROR_BUS;
        }
    }
    return ST7789_OK;
}

static uint8_t rotation_value(ST7789_ROTATION_T rotation)
{
    static const uint8_t values[] = { 0x00U, 0xC0U, 0x70U, 0xA0U };

    return values[(uint32_t)rotation];
}

ST7789_STATUS_T st7789_init(ST7789_T *display,
                            const ST7789_BUS_T *bus,
                            ST7789_ROTATION_T rotation)
{
    ST7789_STATUS_T status;
    uint8_t madctl;

    if ((display == NULL) || (bus == NULL) || (bus->write == NULL) ||
        (bus->set_data_mode == NULL) || (bus->set_reset == NULL) ||
        (bus->delay_ms == NULL) || (rotation > ST7789_ROTATION_270))
    {
        return ST7789_ERROR_INVALID_ARGUMENT;
    }

    display->bus = *bus;
    display->width = ST7789_WIDTH;
    display->height = ST7789_HEIGHT;
    display->x_offset = 0U;
    display->y_offset = 0U;
    display->rotation = rotation;
    display->initialized = false;

    display->bus.set_reset(false);
    display->bus.delay_ms(100U);
    display->bus.set_reset(true);
    display->bus.delay_ms(100U);

    status = write_command(display, ST7789_CMD_SLEEP_OUT, NULL, 0U);
    if (status != ST7789_OK)
    {
        return status;
    }
    display->bus.delay_ms(120U);
    madctl = rotation_value(rotation);

    for (size_t i = 0U; i < (sizeof(s_init_commands) / sizeof(s_init_commands[0])); ++i)
    {
        const uint8_t *data = s_init_commands[i].data;
        if (s_init_commands[i].command == ST7789_CMD_MADCTL)
        {
            data = &madctl;
        }
        status = write_command(display,
                               s_init_commands[i].command,
                               data,
                               s_init_commands[i].length);
        if (status != ST7789_OK)
        {
            return status;
        }
    }

    status = write_command(display, ST7789_CMD_INVERSION_ON, NULL, 0U);
    if (status == ST7789_OK)
    {
        status = write_command(display, ST7789_CMD_DISPLAY_ON, NULL, 0U);
    }
    if (status == ST7789_OK)
    {
        display->bus.delay_ms(20U);
        display->initialized = true;
    }
    return status;
}

ST7789_STATUS_T st7789_set_window(ST7789_T *display,
                                  uint16_t x1,
                                  uint16_t y1,
                                  uint16_t x2,
                                  uint16_t y2)
{
    uint8_t address[4];
    ST7789_STATUS_T status;

    if ((display == NULL) || !display->initialized)
    {
        return ST7789_ERROR_NOT_INITIALIZED;
    }
    if ((x1 > x2) || (y1 > y2) ||
        (x2 >= display->width) || (y2 >= display->height))
    {
        return ST7789_ERROR_OUT_OF_RANGE;
    }

    x1 = (uint16_t)(x1 + display->x_offset);
    x2 = (uint16_t)(x2 + display->x_offset);
    y1 = (uint16_t)(y1 + display->y_offset);
    y2 = (uint16_t)(y2 + display->y_offset);

    address[0] = (uint8_t)(x1 >> 8U);
    address[1] = (uint8_t)x1;
    address[2] = (uint8_t)(x2 >> 8U);
    address[3] = (uint8_t)x2;
    status = write_command(display, ST7789_CMD_COLUMN_ADDR, address, sizeof(address));
    if (status != ST7789_OK)
    {
        return status;
    }

    address[0] = (uint8_t)(y1 >> 8U);
    address[1] = (uint8_t)y1;
    address[2] = (uint8_t)(y2 >> 8U);
    address[3] = (uint8_t)y2;
    status = write_command(display, ST7789_CMD_ROW_ADDR, address, sizeof(address));
    if (status == ST7789_OK)
    {
        status = write_command(display, ST7789_CMD_MEMORY_WRITE, NULL, 0U);
        display->bus.set_data_mode(true);
    }
    return status;
}

/* Static buffers so large chunk sizes do not stress the caller's stack. */
static uint8_t s_pixel_buffer[ST7789_PIXEL_CHUNK * 2U] __attribute__((section(".ccmram")));
static uint16_t s_fill_buffer[ST7789_PIXEL_CHUNK] __attribute__((section(".ccmram")));

ST7789_STATUS_T st7789_write_pixels(ST7789_T *display,
                                    const uint16_t *pixels,
                                    size_t pixel_count)
{
    uint8_t *buffer = s_pixel_buffer;

    if ((display == NULL) || !display->initialized)
    {
        return ST7789_ERROR_NOT_INITIALIZED;
    }
    if ((pixels == NULL) && (pixel_count != 0U))
    {
        return ST7789_ERROR_INVALID_ARGUMENT;
    }

    while (pixel_count != 0U)
    {
        size_t chunk = pixel_count;
        if (chunk > ST7789_PIXEL_CHUNK)
        {
            chunk = ST7789_PIXEL_CHUNK;
        }
        for (size_t i = 0U; i < chunk; ++i)
        {
            buffer[i * 2U] = (uint8_t)(pixels[i] >> 8U);
            buffer[i * 2U + 1U] = (uint8_t)pixels[i];
        }
        if (!display->bus.write(buffer, chunk * 2U))
        {
            return ST7789_ERROR_BUS;
        }
        pixels += chunk;
        pixel_count -= chunk;
    }
    return ST7789_OK;
}

ST7789_STATUS_T st7789_fill(ST7789_T *display,
                            uint16_t color,
                            size_t pixel_count)
{
    uint16_t *pixels = s_fill_buffer;

    for (size_t i = 0U; i < ST7789_PIXEL_CHUNK; ++i)
    {
        pixels[i] = color;
    }
    while (pixel_count != 0U)
    {
        size_t chunk = pixel_count;
        ST7789_STATUS_T status;
        if (chunk > ST7789_PIXEL_CHUNK)
        {
            chunk = ST7789_PIXEL_CHUNK;
        }
        status = st7789_write_pixels(display, pixels, chunk);
        if (status != ST7789_OK)
        {
            return status;
        }
        pixel_count -= chunk;
    }
    return ST7789_OK;
}
