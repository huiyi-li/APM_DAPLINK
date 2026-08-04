#ifndef BSP_W25QXX_H
#define BSP_W25QXX_H

#include <stddef.h>
#include <stdint.h>

#define BSP_W25Q64_JEDEC_ID       0xEF4017U
#define BSP_W25Q64_CAPACITY       (8U * 1024U * 1024U)
#define BSP_W25Q64_SECTOR_SIZE    4096U
#define BSP_W25Q64_PAGE_SIZE      256U

typedef enum
{
    BSP_W25Q64_OK = 0,
    BSP_W25Q64_ERROR_INVALID_ARGUMENT,
    BSP_W25Q64_ERROR_NOT_INITIALIZED,
    BSP_W25Q64_ERROR_NOT_FOUND,
    BSP_W25Q64_ERROR_OUT_OF_RANGE,
    BSP_W25Q64_ERROR_TIMEOUT,
    BSP_W25Q64_ERROR_VERIFY
} BSP_W25Q64_STATUS_T;

BSP_W25Q64_STATUS_T bsp_w25q64_init(void);
BSP_W25Q64_STATUS_T bsp_w25q64_read_jedec_id(uint32_t *jedec_id);
BSP_W25Q64_STATUS_T bsp_w25q64_read(uint32_t address, void *data, size_t size);
BSP_W25Q64_STATUS_T bsp_w25q64_page_program(uint32_t address,
                                            const void *data,
                                            size_t size);
BSP_W25Q64_STATUS_T bsp_w25q64_erase_sector(uint32_t address);
BSP_W25Q64_STATUS_T bsp_w25q64_write(uint32_t address,
                                     const void *data,
                                     size_t size);
BSP_W25Q64_STATUS_T bsp_w25q64_chip_erase(void);
BSP_W25Q64_STATUS_T bsp_w25q64_power_down(void);
BSP_W25Q64_STATUS_T bsp_w25q64_wakeup(void);

#endif
