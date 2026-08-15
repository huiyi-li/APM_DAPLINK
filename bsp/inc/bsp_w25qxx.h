#ifndef BSP_W25QXX_H
#define BSP_W25QXX_H

#include <stddef.h>
#include <stdint.h>

/*
 * Winbond W25Q series SPI NOR flash driver.
 *
 * Chip selection: enable exactly one of the BSP_W25QXX_CHIP_* macros,
 * or override the BSP_W25QXX_* configuration macros directly to adapt
 * other densities/vendors (same JEDEC command set, e.g. GD25Q, ZB25Q).
 *
 * Hardware layer: SPI1 (CS PA4 / SCK PA5 / MISO PA6 / MOSI PA7,
 * WP tied to VCC, /HOLD tied to GND). The protocol layer is independent
 * of the transport, so switching to another MCU only requires replacing
 * the low-level transfer functions.
 */

/* Chip selection (enable exactly one) */
#define BSP_W25QXX_CHIP_W25Q64   0U
#define BSP_W25QXX_CHIP_W25Q128  1U

/*
 * SPI transport selection:
 *   0 = hardware SPI1 (CS PA4 / SCK PA5 / MISO PA6 / MOSI PA7)
 *   1 = software bit-bang, DI on PA6 / DO on PA7 (usable when the board
 *       routes flash DI to SPI1_MISO and DO to SPI1_MOSI)
 */
#ifndef BSP_W25QXX_USE_SW_SPI
#define BSP_W25QXX_USE_SW_SPI    0U
#endif

#if BSP_W25QXX_CHIP_W25Q64
#define BSP_W25QXX_JEDEC_ID       0xEF4017U
#define BSP_W25QXX_CAPACITY       (8U * 1024U * 1024U)
#elif BSP_W25QXX_CHIP_W25Q128
#define BSP_W25QXX_JEDEC_ID       0xEF4018U
#define BSP_W25QXX_CAPACITY       (16U * 1024U * 1024U)
#else
#error "Select one BSP_W25QXX_CHIP_* macro"
#endif

/* Geometry (same for the whole W25Q family) */
#define BSP_W25QXX_SECTOR_SIZE    4096U
#define BSP_W25QXX_PAGE_SIZE      256U
#define BSP_W25QXX_ADDR_BYTES     3U   /* 3 for up to 16MB, 4 for >16MB */

typedef enum
{
    BSP_W25QXX_OK = 0,
    BSP_W25QXX_ERROR_INVALID_ARGUMENT,
    BSP_W25QXX_ERROR_NOT_INITIALIZED,
    BSP_W25QXX_ERROR_NOT_FOUND,
    BSP_W25QXX_ERROR_OUT_OF_RANGE,
    BSP_W25QXX_ERROR_TIMEOUT,
    BSP_W25QXX_ERROR_VERIFY
} BSP_W25QXX_STATUS_T;

BSP_W25QXX_STATUS_T bsp_w25qxx_init(void);
BSP_W25QXX_STATUS_T bsp_w25qxx_read_jedec_id(uint32_t *jedec_id);
BSP_W25QXX_STATUS_T bsp_w25qxx_read(uint32_t address, void *data, size_t size);
BSP_W25QXX_STATUS_T bsp_w25qxx_page_program(uint32_t address,
                                            const void *data,
                                            size_t size);
BSP_W25QXX_STATUS_T bsp_w25qxx_erase_sector(uint32_t address);
BSP_W25QXX_STATUS_T bsp_w25qxx_write(uint32_t address,
                                     const void *data,
                                     size_t size);
BSP_W25QXX_STATUS_T bsp_w25qxx_chip_erase(void);
BSP_W25QXX_STATUS_T bsp_w25qxx_power_down(void);
BSP_W25QXX_STATUS_T bsp_w25qxx_wakeup(void);

#endif
