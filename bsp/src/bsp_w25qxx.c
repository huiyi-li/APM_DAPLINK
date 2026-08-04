#include "bsp_w25qxx.h"

#include <stdbool.h>
#include <string.h>

#include "apm32f4xx.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "apm32f4xx_spi.h"
#include "board.h"

#define W25Q64_CMD_WRITE_ENABLE     0x06U
#define W25Q64_CMD_READ_STATUS_1    0x05U
#define W25Q64_CMD_READ_DATA        0x03U
#define W25Q64_CMD_PAGE_PROGRAM     0x02U
#define W25Q64_CMD_SECTOR_ERASE     0x20U
#define W25Q64_CMD_CHIP_ERASE       0xC7U
#define W25Q64_CMD_POWER_DOWN       0xB9U
#define W25Q64_CMD_RELEASE_POWER    0xABU
#define W25Q64_CMD_READ_JEDEC_ID    0x9FU

#define W25Q64_STATUS_BUSY          (1U << 0)
#define W25Q64_STATUS_WEL           (1U << 1)

#define W25Q64_SPI_TIMEOUT_MS       10U
#define W25Q64_PAGE_TIMEOUT_MS      100U
#define W25Q64_SECTOR_TIMEOUT_MS    3000U
#define W25Q64_CHIP_TIMEOUT_MS      200000U

typedef struct
{
    uint32_t previous_cycles;
    uint64_t elapsed_cycles;
    uint64_t limit_cycles;
} W25Q64_TIMEOUT_T;

static bool s_bus_ready;
static bool s_initialized;
static uint8_t s_sector_buffer[BSP_W25Q64_SECTOR_SIZE];
static uint8_t s_verify_buffer[BSP_W25Q64_PAGE_SIZE];

static void w25q64_select(void)
{
    GPIO_ResetBit(BOARD_FLASH_CS_PORT, BOARD_FLASH_CS_PIN);
}

static void w25q64_deselect(void)
{
    GPIO_SetBit(BOARD_FLASH_CS_PORT, BOARD_FLASH_CS_PIN);
}

static void w25q64_timeout_start(W25Q64_TIMEOUT_T *timeout, uint32_t timeout_ms)
{
    timeout->previous_cycles = DWT->CYCCNT;
    timeout->elapsed_cycles = 0U;
    timeout->limit_cycles = ((uint64_t)SystemCoreClock * timeout_ms) / 1000U;
}

static bool w25q64_timeout_expired(W25Q64_TIMEOUT_T *timeout)
{
    const uint32_t current_cycles = DWT->CYCCNT;

    timeout->elapsed_cycles += (uint32_t)(current_cycles - timeout->previous_cycles);
    timeout->previous_cycles = current_cycles;
    return timeout->elapsed_cycles >= timeout->limit_cycles;
}

static void w25q64_delay_us(uint32_t delay_us)
{
    W25Q64_TIMEOUT_T timeout;

    w25q64_timeout_start(&timeout, 1U);
    timeout.limit_cycles = ((uint64_t)SystemCoreClock * delay_us) / 1000000U;
    while (!w25q64_timeout_expired(&timeout))
    {
    }
}

static BSP_W25Q64_STATUS_T w25q64_transfer(uint8_t tx_data, uint8_t *rx_data)
{
    W25Q64_TIMEOUT_T timeout;

    w25q64_timeout_start(&timeout, W25Q64_SPI_TIMEOUT_MS);
    while (SPI_I2S_ReadStatusFlag(SPI1, SPI_FLAG_TXBE) == RESET)
    {
        if (w25q64_timeout_expired(&timeout))
        {
            return BSP_W25Q64_ERROR_TIMEOUT;
        }
    }

    SPI1->DATA = tx_data;

    w25q64_timeout_start(&timeout, W25Q64_SPI_TIMEOUT_MS);
    while (SPI_I2S_ReadStatusFlag(SPI1, SPI_FLAG_RXBNE) == RESET)
    {
        if (w25q64_timeout_expired(&timeout))
        {
            return BSP_W25Q64_ERROR_TIMEOUT;
        }
    }

    if (rx_data != NULL)
    {
        *rx_data = (uint8_t)SPI1->DATA;
    }
    else
    {
        (void)SPI1->DATA;
    }

    return BSP_W25Q64_OK;
}

static BSP_W25Q64_STATUS_T w25q64_send_address(uint32_t address)
{
    BSP_W25Q64_STATUS_T status;

    status = w25q64_transfer((uint8_t)(address >> 16U), NULL);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_transfer((uint8_t)(address >> 8U), NULL);
    }
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_transfer((uint8_t)address, NULL);
    }
    return status;
}

static BSP_W25Q64_STATUS_T w25q64_read_status(uint8_t *status_register)
{
    BSP_W25Q64_STATUS_T status;

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_READ_STATUS_1, NULL);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_transfer(0xFFU, status_register);
    }
    w25q64_deselect();
    return status;
}

static BSP_W25Q64_STATUS_T w25q64_wait_ready(uint32_t timeout_ms)
{
    W25Q64_TIMEOUT_T timeout;
    BSP_W25Q64_STATUS_T status;
    uint8_t status_register;

    w25q64_timeout_start(&timeout, timeout_ms);
    do
    {
        status = w25q64_read_status(&status_register);
        if (status != BSP_W25Q64_OK)
        {
            return status;
        }
        if ((status_register & W25Q64_STATUS_BUSY) == 0U)
        {
            return BSP_W25Q64_OK;
        }
    } while (!w25q64_timeout_expired(&timeout));

    return BSP_W25Q64_ERROR_TIMEOUT;
}

static BSP_W25Q64_STATUS_T w25q64_write_enable(void)
{
    BSP_W25Q64_STATUS_T status;
    uint8_t status_register;

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_WRITE_ENABLE, NULL);
    w25q64_deselect();
    if (status != BSP_W25Q64_OK)
    {
        return status;
    }

    status = w25q64_read_status(&status_register);
    if ((status == BSP_W25Q64_OK) &&
        ((status_register & W25Q64_STATUS_WEL) == 0U))
    {
        return BSP_W25Q64_ERROR_VERIFY;
    }
    return status;
}

static bool w25q64_range_valid(uint32_t address, size_t size)
{
    return (address <= BSP_W25Q64_CAPACITY) &&
           (size <= (size_t)(BSP_W25Q64_CAPACITY - address));
}

static BSP_W25Q64_STATUS_T w25q64_program_pages(uint32_t address,
                                                const uint8_t *data,
                                                size_t size)
{
    BSP_W25Q64_STATUS_T status;

    while (size != 0U)
    {
        size_t chunk_size = BSP_W25Q64_PAGE_SIZE -
                            (address % BSP_W25Q64_PAGE_SIZE);
        if (chunk_size > size)
        {
            chunk_size = size;
        }

        status = bsp_w25q64_page_program(address, data, chunk_size);
        if (status != BSP_W25Q64_OK)
        {
            return status;
        }
        address += (uint32_t)chunk_size;
        data += chunk_size;
        size -= chunk_size;
    }
    return BSP_W25Q64_OK;
}

static BSP_W25Q64_STATUS_T w25q64_verify(uint32_t address,
                                         const uint8_t *expected,
                                         size_t size)
{
    BSP_W25Q64_STATUS_T status;

    while (size != 0U)
    {
        size_t chunk_size = size;
        if (chunk_size > sizeof(s_verify_buffer))
        {
            chunk_size = sizeof(s_verify_buffer);
        }
        status = bsp_w25q64_read(address, s_verify_buffer, chunk_size);
        if (status != BSP_W25Q64_OK)
        {
            return status;
        }
        if (memcmp(s_verify_buffer, expected, chunk_size) != 0)
        {
            return BSP_W25Q64_ERROR_VERIFY;
        }
        address += (uint32_t)chunk_size;
        expected += chunk_size;
        size -= chunk_size;
    }
    return BSP_W25Q64_OK;
}

BSP_W25Q64_STATUS_T bsp_w25q64_init(void)
{
    GPIO_Config_T gpio_config;
    SPI_Config_T spi_config;
    uint32_t jedec_id;

    s_bus_ready = false;
    s_initialized = false;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    RCM_EnableAHB1PeriphClock(BOARD_FLASH_GPIO_CLOCK);
    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SPI1);

    w25q64_deselect();
    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_FLASH_CS_PIN;
    gpio_config.mode = GPIO_MODE_OUT;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_UP;
    GPIO_Config(BOARD_FLASH_CS_PORT, &gpio_config);

    GPIO_ConfigPinAF(BOARD_FLASH_SCK_PORT,
                     BOARD_FLASH_SCK_PIN_SOURCE,
                     GPIO_AF_SPI1);
    GPIO_ConfigPinAF(BOARD_FLASH_MISO_PORT,
                     BOARD_FLASH_MISO_PIN_SOURCE,
                     GPIO_AF_SPI1);
    GPIO_ConfigPinAF(BOARD_FLASH_MOSI_PORT,
                     BOARD_FLASH_MOSI_PIN_SOURCE,
                     GPIO_AF_SPI1);

    GPIO_ConfigStructInit(&gpio_config);
    gpio_config.pin = BOARD_FLASH_SCK_PIN |
                      BOARD_FLASH_MISO_PIN |
                      BOARD_FLASH_MOSI_PIN;
    gpio_config.mode = GPIO_MODE_AF;
    gpio_config.speed = GPIO_SPEED_50MHz;
    gpio_config.otype = GPIO_OTYPE_PP;
    gpio_config.pupd = GPIO_PUPD_NOPULL;
    GPIO_Config(BOARD_FLASH_SCK_PORT, &gpio_config);

    SPI_ConfigStructInit(&spi_config);
    spi_config.mode = SPI_MODE_MASTER;
    spi_config.direction = SPI_DIRECTION_2LINES_FULLDUPLEX;
    spi_config.length = SPI_DATA_LENGTH_8B;
    spi_config.polarity = SPI_CLKPOL_LOW;
    spi_config.phase = SPI_CLKPHA_1EDGE;
    spi_config.nss = SPI_NSS_SOFT;
    spi_config.baudrateDiv = SPI_BAUDRATE_DIV_4;
    spi_config.firstBit = SPI_FIRSTBIT_MSB;
    spi_config.crcPolynomial = 7U;
    SPI_Config(SPI1, &spi_config);
    SPI_Enable(SPI1);
    s_bus_ready = true;

    (void)bsp_w25q64_wakeup();
    if (bsp_w25q64_read_jedec_id(&jedec_id) != BSP_W25Q64_OK)
    {
        return BSP_W25Q64_ERROR_TIMEOUT;
    }
    if (jedec_id != BSP_W25Q64_JEDEC_ID)
    {
        return BSP_W25Q64_ERROR_NOT_FOUND;
    }

    s_initialized = true;
    return BSP_W25Q64_OK;
}

BSP_W25Q64_STATUS_T bsp_w25q64_read_jedec_id(uint32_t *jedec_id)
{
    BSP_W25Q64_STATUS_T status;
    uint8_t id[3];

    if (jedec_id == NULL)
    {
        return BSP_W25Q64_ERROR_INVALID_ARGUMENT;
    }
    if (!s_bus_ready)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_READ_JEDEC_ID, NULL);
    for (size_t i = 0U; (i < sizeof(id)) && (status == BSP_W25Q64_OK); ++i)
    {
        status = w25q64_transfer(0xFFU, &id[i]);
    }
    w25q64_deselect();

    if (status == BSP_W25Q64_OK)
    {
        *jedec_id = ((uint32_t)id[0] << 16U) |
                    ((uint32_t)id[1] << 8U) |
                    id[2];
    }
    return status;
}

BSP_W25Q64_STATUS_T bsp_w25q64_read(uint32_t address, void *data, size_t size)
{
    BSP_W25Q64_STATUS_T status;
    uint8_t *output = data;

    if ((data == NULL) && (size != 0U))
    {
        return BSP_W25Q64_ERROR_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    if (!w25q64_range_valid(address, size))
    {
        return BSP_W25Q64_ERROR_OUT_OF_RANGE;
    }
    if (size == 0U)
    {
        return BSP_W25Q64_OK;
    }

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_READ_DATA, NULL);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_send_address(address);
    }
    for (size_t i = 0U; (i < size) && (status == BSP_W25Q64_OK); ++i)
    {
        status = w25q64_transfer(0xFFU, &output[i]);
    }
    w25q64_deselect();
    return status;
}

BSP_W25Q64_STATUS_T bsp_w25q64_page_program(uint32_t address,
                                            const void *data,
                                            size_t size)
{
    BSP_W25Q64_STATUS_T status;
    const uint8_t *input = data;

    if ((data == NULL) && (size != 0U))
    {
        return BSP_W25Q64_ERROR_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    if (!w25q64_range_valid(address, size) ||
        (size > BSP_W25Q64_PAGE_SIZE) ||
        (((address % BSP_W25Q64_PAGE_SIZE) + size) > BSP_W25Q64_PAGE_SIZE))
    {
        return BSP_W25Q64_ERROR_OUT_OF_RANGE;
    }
    if (size == 0U)
    {
        return BSP_W25Q64_OK;
    }

    status = w25q64_wait_ready(W25Q64_PAGE_TIMEOUT_MS);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_write_enable();
    }
    if (status != BSP_W25Q64_OK)
    {
        return status;
    }

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_PAGE_PROGRAM, NULL);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_send_address(address);
    }
    for (size_t i = 0U; (i < size) && (status == BSP_W25Q64_OK); ++i)
    {
        status = w25q64_transfer(input[i], NULL);
    }
    w25q64_deselect();

    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_wait_ready(W25Q64_PAGE_TIMEOUT_MS);
    }
    return status;
}

BSP_W25Q64_STATUS_T bsp_w25q64_erase_sector(uint32_t address)
{
    BSP_W25Q64_STATUS_T status;

    if (!s_initialized)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    if (address >= BSP_W25Q64_CAPACITY)
    {
        return BSP_W25Q64_ERROR_OUT_OF_RANGE;
    }
    address -= address % BSP_W25Q64_SECTOR_SIZE;

    status = w25q64_wait_ready(W25Q64_SECTOR_TIMEOUT_MS);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_write_enable();
    }
    if (status != BSP_W25Q64_OK)
    {
        return status;
    }

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_SECTOR_ERASE, NULL);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_send_address(address);
    }
    w25q64_deselect();

    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_wait_ready(W25Q64_SECTOR_TIMEOUT_MS);
    }
    return status;
}

BSP_W25Q64_STATUS_T bsp_w25q64_write(uint32_t address,
                                     const void *data,
                                     size_t size)
{
    const uint8_t *input = data;

    if ((data == NULL) && (size != 0U))
    {
        return BSP_W25Q64_ERROR_INVALID_ARGUMENT;
    }
    if (!s_initialized)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    if (!w25q64_range_valid(address, size))
    {
        return BSP_W25Q64_ERROR_OUT_OF_RANGE;
    }

    while (size != 0U)
    {
        const uint32_t sector_address =
            address - (address % BSP_W25Q64_SECTOR_SIZE);
        const size_t sector_offset = address - sector_address;
        size_t chunk_size = BSP_W25Q64_SECTOR_SIZE - sector_offset;
        BSP_W25Q64_STATUS_T status;
        bool erase_required = false;

        if (chunk_size > size)
        {
            chunk_size = size;
        }
        status = bsp_w25q64_read(sector_address,
                                 s_sector_buffer,
                                 sizeof(s_sector_buffer));
        if (status != BSP_W25Q64_OK)
        {
            return status;
        }

        for (size_t i = 0U; i < chunk_size; ++i)
        {
            if ((s_sector_buffer[sector_offset + i] & input[i]) != input[i])
            {
                erase_required = true;
                break;
            }
        }

        if (erase_required)
        {
            memcpy(&s_sector_buffer[sector_offset], input, chunk_size);
            status = bsp_w25q64_erase_sector(sector_address);
            if (status == BSP_W25Q64_OK)
            {
                status = w25q64_program_pages(sector_address,
                                               s_sector_buffer,
                                               sizeof(s_sector_buffer));
            }
        }
        else
        {
            status = w25q64_program_pages(address, input, chunk_size);
        }
        if (status == BSP_W25Q64_OK)
        {
            status = w25q64_verify(address, input, chunk_size);
        }
        if (status != BSP_W25Q64_OK)
        {
            return status;
        }

        address += (uint32_t)chunk_size;
        input += chunk_size;
        size -= chunk_size;
    }
    return BSP_W25Q64_OK;
}

BSP_W25Q64_STATUS_T bsp_w25q64_chip_erase(void)
{
    BSP_W25Q64_STATUS_T status;

    if (!s_initialized)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    status = w25q64_wait_ready(W25Q64_SECTOR_TIMEOUT_MS);
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_write_enable();
    }
    if (status != BSP_W25Q64_OK)
    {
        return status;
    }

    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_CHIP_ERASE, NULL);
    w25q64_deselect();
    if (status == BSP_W25Q64_OK)
    {
        status = w25q64_wait_ready(W25Q64_CHIP_TIMEOUT_MS);
    }
    return status;
}

BSP_W25Q64_STATUS_T bsp_w25q64_power_down(void)
{
    BSP_W25Q64_STATUS_T status;

    if (!s_initialized)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    status = w25q64_wait_ready(W25Q64_SECTOR_TIMEOUT_MS);
    if (status != BSP_W25Q64_OK)
    {
        return status;
    }
    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_POWER_DOWN, NULL);
    w25q64_deselect();
    w25q64_delay_us(3U);
    return status;
}

BSP_W25Q64_STATUS_T bsp_w25q64_wakeup(void)
{
    BSP_W25Q64_STATUS_T status;

    if (!s_bus_ready)
    {
        return BSP_W25Q64_ERROR_NOT_INITIALIZED;
    }
    w25q64_select();
    status = w25q64_transfer(W25Q64_CMD_RELEASE_POWER, NULL);
    w25q64_deselect();
    w25q64_delay_us(3U);
    return status;
}
