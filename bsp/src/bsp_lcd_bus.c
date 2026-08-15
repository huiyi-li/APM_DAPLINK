#include "bsp_lcd_bus.h"

#include "apm32f4xx.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#if BSP_LCD_BUS_USE_HW_SPI
#include "apm32f4xx_spi.h"
#endif
#include "board.h"

/*
 * SPI transport for the LCD panel, selectable at compile time:
 *   BSP_LCD_BUS_USE_HW_SPI = 1 -> hardware SPI3 master (PC10 SCK / PC12 MOSI)
 *   BSP_LCD_BUS_USE_HW_SPI = 0 -> software bit-bang SPI (same pins)
 *
 * Signal mapping (see board.h):
 *   CS   - active low chip select (GPIO), held low for the whole transaction
 *   DC   - data/command select (GPIO), 1 = data, 0 = command
 *   SCL  - SPI clock, idle low, data sampled on the rising edge
 *   SDA  - SPI MOSI, MSB first
 *   RES  - hardware reset (GPIO), active low
 *
 * The bus layer only knows about SPI timing; LCD specific command
 * sequences live in the panel driver (st7789.c) behind the same
 * BSP_LCD_BUS interface, so a different panel can be swapped in by
 * re-implementing this interface without touching the display logic.
 */

#if BSP_LCD_BUS_USE_HW_SPI
#define LCD_SPI_TIMEOUT_MS 10U
#endif

static bool s_initialized;

static void lcd_pin_set(GPIO_T *port, uint16_t pin)
{
    port->BSCL = pin;
}

static void lcd_pin_clear(GPIO_T *port, uint16_t pin)
{
    port->BSCH = pin;
}

#if BSP_LCD_BUS_USE_HW_SPI

static bool lcd_bus_wait_flag(SPI_FLAG_T flag, uint8_t expected)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t limit = SystemCoreClock / (1000U / LCD_SPI_TIMEOUT_MS);

    while (SPI_I2S_ReadStatusFlag(SPI3, flag) != expected)
    {
        if ((uint32_t)(DWT->CYCCNT - start) >= limit)
        {
            return false;
        }
    }
    return true;
}

static BSP_LCD_BUS_STATUS_T lcd_bus_transport_init(void)
{
    GPIO_Config_T gpio_config;
    SPI_Config_T spi_config;

    RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_SPI3);

    GPIO_ConfigPinAF(BOARD_LCD_SCK_PORT,
                     BOARD_LCD_SCK_PIN_SOURCE,
                     GPIO_AF_SPI3);
    GPIO_ConfigPinAF(BOARD_LCD_MOSI_PORT,
                     BOARD_LCD_MOSI_PIN_SOURCE,
                     GPIO_AF_SPI3);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_LCD_SCK_PIN | BOARD_LCD_MOSI_PIN;
    gpio_config.mode = GPIO_MODE_AF;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_NOPULL;
    GPIO_Config(BOARD_LCD_SCK_PORT, &gpio_config);

    SPI_ConfigStructInit(&spi_config);
    spi_config.mode = SPI_MODE_MASTER;
    spi_config.direction = SPI_DIRECTION_1LINE_TX;
    spi_config.length = SPI_DATA_LENGTH_8B;
    spi_config.polarity = SPI_CLKPOL_LOW;
    spi_config.phase = SPI_CLKPHA_1EDGE;
    spi_config.nss = SPI_NSS_SOFT;
    spi_config.baudrateDiv = SPI_BAUDRATE_DIV_2;
    spi_config.firstBit = SPI_FIRSTBIT_MSB;
    spi_config.crcPolynomial = 7U;
    SPI_Config(SPI3, &spi_config);
    SPI_Enable(SPI3);
    return BSP_LCD_BUS_OK;
}

static BSP_LCD_BUS_STATUS_T lcd_bus_transport_write(const uint8_t *data, size_t size)
{
    for (size_t i = 0U; i < size; ++i)
    {
        if (!lcd_bus_wait_flag(SPI_FLAG_TXBE, SET))
        {
            return BSP_LCD_BUS_ERROR_TIMEOUT;
        }
        SPI3->DATA = data[i];
    }

    if (!lcd_bus_wait_flag(SPI_FLAG_TXBE, SET) ||
        !lcd_bus_wait_flag(SPI_FLAG_BSY, RESET))
    {
        return BSP_LCD_BUS_ERROR_TIMEOUT;
    }
    return BSP_LCD_BUS_OK;
}

#else /* software bit-bang SPI */

static BSP_LCD_BUS_STATUS_T lcd_bus_transport_init(void)
{
    GPIO_Config_T gpio_config;

    /* SCL idle low, SDA idle low. */
    lcd_pin_clear(BOARD_LCD_SCK_PORT, BOARD_LCD_SCK_PIN);
    lcd_pin_clear(BOARD_LCD_MOSI_PORT, BOARD_LCD_MOSI_PIN);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_LCD_SCK_PIN | BOARD_LCD_MOSI_PIN;
    gpio_config.mode = GPIO_MODE_OUT;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(BOARD_LCD_SCK_PORT, &gpio_config);
    return BSP_LCD_BUS_OK;
}

static void lcd_byte_out(uint8_t data)
{
    for (uint8_t i = 0U; i < 8U; ++i)
    {
        lcd_pin_clear(BOARD_LCD_SCK_PORT, BOARD_LCD_SCK_PIN);
        if ((data & 0x80U) != 0U)
        {
            lcd_pin_set(BOARD_LCD_MOSI_PORT, BOARD_LCD_MOSI_PIN);
        }
        else
        {
            lcd_pin_clear(BOARD_LCD_MOSI_PORT, BOARD_LCD_MOSI_PIN);
        }
        lcd_pin_set(BOARD_LCD_SCK_PORT, BOARD_LCD_SCK_PIN);
        data <<= 1U;
    }
}

static BSP_LCD_BUS_STATUS_T lcd_bus_transport_write(const uint8_t *data, size_t size)
{
    for (size_t i = 0U; i < size; ++i)
    {
        lcd_byte_out(data[i]);
    }
    return BSP_LCD_BUS_OK;
}

#endif

BSP_LCD_BUS_STATUS_T bsp_lcd_bus_init(void)
{
    GPIO_Config_T gpio_config;

    s_initialized = false;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    RCM_EnableAHB1PeriphClock(BOARD_LCD_GPIO_CLOCK);
    RCM_EnableAHB1PeriphClock(BOARD_LCD_CS_GPIO_CLOCK);

    /* Idle levels: CS high (deselected), DC data, RES high (out of reset). */
    lcd_pin_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);
    lcd_pin_set(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
    lcd_pin_set(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_LCD_CS_PIN;
    gpio_config.mode = GPIO_MODE_OUT;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(BOARD_LCD_CS_PORT, &gpio_config);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_LCD_DC_PIN | BOARD_LCD_RESET_PIN;
    gpio_config.mode = GPIO_MODE_OUT;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(BOARD_LCD_DC_PORT, &gpio_config);

    if (lcd_bus_transport_init() != BSP_LCD_BUS_OK)
    {
        return BSP_LCD_BUS_ERROR_DRIVER;
    }

    s_initialized = true;
    return BSP_LCD_BUS_OK;
}

void bsp_lcd_bus_set_reset(bool high)
{
    if (high)
    {
        lcd_pin_set(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    }
    else
    {
        lcd_pin_clear(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    }
}

void bsp_lcd_bus_set_data_mode(bool data_mode)
{
    if (data_mode)
    {
        lcd_pin_set(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
    }
    else
    {
        lcd_pin_clear(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
    }
}

BSP_LCD_BUS_STATUS_T bsp_lcd_bus_write(const uint8_t *data, size_t size)
{
    BSP_LCD_BUS_STATUS_T status;

    if ((data == NULL) && (size != 0U))
    {
        return BSP_LCD_BUS_ERROR_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return BSP_LCD_BUS_ERROR_NOT_INITIALIZED;
    }
    if (size == 0U)
    {
        return BSP_LCD_BUS_OK;
    }

    lcd_pin_clear(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);
    status = lcd_bus_transport_write(data, size);
    lcd_pin_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);

    return status;
}
