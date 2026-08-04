#include "bsp_lcd_bus.h"

#include "apm32f4xx.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "apm32f4xx_spi.h"
#include "board.h"

#define LCD_SPI_TIMEOUT_MS 10U

static bool s_initialized;

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

BSP_LCD_BUS_STATUS_T bsp_lcd_bus_init(void)
{
    GPIO_Config_T gpio_config;
    SPI_Config_T spi_config;

    s_initialized = false;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    RCM_EnableAHB1PeriphClock(BOARD_LCD_GPIO_CLOCK);
    RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_SPI3);

    GPIO_SetBit(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
    GPIO_SetBit(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_LCD_DC_PIN | BOARD_LCD_RESET_PIN;
    gpio_config.mode = GPIO_MODE_OUT;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(BOARD_LCD_DC_PORT, &gpio_config);

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

    s_initialized = true;
    return BSP_LCD_BUS_OK;
}

void bsp_lcd_bus_set_reset(bool high)
{
    if (high)
    {
        GPIO_SetBit(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    }
    else
    {
        GPIO_ResetBit(BOARD_LCD_RESET_PORT, BOARD_LCD_RESET_PIN);
    }
}

void bsp_lcd_bus_set_data_mode(bool data_mode)
{
    if (data_mode)
    {
        GPIO_SetBit(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
    }
    else
    {
        GPIO_ResetBit(BOARD_LCD_DC_PORT, BOARD_LCD_DC_PIN);
    }
}

BSP_LCD_BUS_STATUS_T bsp_lcd_bus_write(const uint8_t *data, size_t size)
{
    if ((data == NULL) && (size != 0U))
    {
        return BSP_LCD_BUS_ERROR_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return BSP_LCD_BUS_ERROR_NOT_INITIALIZED;
    }

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
