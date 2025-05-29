/*!
 * @file        bsp_w25qxx.h
 *
 * @brief       Header for bsp_w25qxx.c module
 *
 * @version     V1.0.0
 *
 * @date        2023-03-27
 *
 * @attention
 *
 *  Copyright (C) 2023 Geehy Semiconductor
 *
 *  You may not use this file except in compliance with the
 *  GEEHY COPYRIGHT NOTICE (GEEHY SOFTWARE PACKAGE LICENSE).
 *
 *  The program is only for reference, which is distributed in the hope
 *  that it will be usefull and instructional for customers to develop
 *  their software. Unless required by applicable law or agreed to in
 *  writing, the program is distributed on an "AS IS" BASIS, WITHOUT
 *  ANY WARRANTY OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the GEEHY SOFTWARE PACKAGE LICENSE for the governing permissions
 *  and limitations under the License.
 */

/* Define to prevent recursive inclusion */
#ifndef __BSP_W25QXX_H
#define __BSP_W25QXX_H

#ifdef __cplusplus
  extern "C" {
#endif

/* Includes */
#include "apm32f4xx.h"
#include "apm32f4xx_rcm.h"
#include "apm32f4xx_spi.h"
#include "apm32f4xx_gpio.h"
#include "bsp_delay.h"

/** @addtogroup Board
  @{
*/

/** @defgroup Board_APM32F407_EVAL
  @{
*/

/** @defgroup BSP_W25QXX_Macros Macros
  @{
*/

/* W25QXX Flash ID */
#define W25Q80_FLASH_ID                     0xEF13U
#define W25Q16_FLASH_ID                     0xEF14U
#define W25Q32_FLASH_ID                     0xEF15U
#define W25Q64_FLASH_ID                     0xEF16U
#define W25Q128_FLASH_ID                    0xEF17U
#define W25Q256_FLASH_ID                    0xEF18U

/* W25QXX Flash Command Table */
#define W25QXX_CMD_ENABLE_WRITE             0x06U
#define W25QXX_CMD_DISABLE_WRITE            0x04U
#define W25QXX_CMD_READ_STA_REG1            0x05U
#define W25QXX_CMD_READ_STA_REG2            0x35U
#define W25QXX_CMD_READ_STA_REG3            0x15U
#define W25QXX_CMD_WRITE_STA_REG1           0x01U
#define W25QXX_CMD_WRITE_STA_REG2           0x31U
#define W25QXX_CMD_WRITE_STA_REG3           0x11U
#define W25QXX_CMD_READ_DATA                0x03U
#define W25QXX_CMD_FAST_READ_DATA           0x0BU
#define W25QXX_CMD_FAST_READ_DUAL           0x3BU
#define W25QXX_CMD_PAGE_PROGRAM             0x02U
#define W25QXX_CMD_BLOCK_ERASE              0xD8U
#define W25QXX_CMD_SECTOR_ERASE             0x20U
#define W25QXX_CMD_CHIP_ERASE               0xC7U
#define W25QXX_CMD_PWR_DOWN                 0xB9U
#define W25QXX_CMD_REL_PWR_DOWN             0xABU
#define W25QXX_CMD_DEV_ID                   0xABU
#define W25QXX_MFC_DEV_ID                   0x90U
#define W25QXX_JEDEC_DEV_ID                 0x9FU
#define W25QXX_ENABLE_4_BYTE_ADDR           0xB7U
#define W25QXX_EXIT_4_BYTE_ADDR             0xE9U

/* W25QXX Flash GPIO */
#define W25QXX_SPI                          SPI1
#define W25QXX_SPI_CLK_ENABLE()             RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SPI1)
#define W25QXX_SPI_GPIO_CLK_ENABLE()        RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOB)
#define W25QXX_SPI_GPIO_PORT                GPIOB
#define W25QXX_SPI_GPIO_AF                  GPIO_AF_SPI1
#define W25QXX_SPI_SCK_GPIO_PIN             GPIO_PIN_3
#define W25QXX_SPI_MISO_GPIO_PIN            GPIO_PIN_4
#define W25QXX_SPI_MOSI_GPIO_PIN            GPIO_PIN_5

#define W25QXX_CS_GPIO_CLK_ENABLE()         RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOF)
#define W25QXX_CS_GPIO_PORT                 GPIOF
#define W25QXX_CS_GPIO_PIN                  GPIO_PIN_5

#define W25QXX_CS_Enable()                  GPIO_ResetBit(W25QXX_CS_GPIO_PORT, W25QXX_CS_GPIO_PIN)
#define W25QXX_CS_Disable()                 GPIO_SetBit(W25QXX_CS_GPIO_PORT, W25QXX_CS_GPIO_PIN)

/* W25QXX Delay */
#define W25QXX_Delay(tick)                  APM_DelayMs(tick)

/* W25QXX Flash Information */
#define W25QXX_SECTOR_SIZE                  4096U

/**@} end of group BSP_W25QXX_Macros*/

/** @defgroup BSP_W25QXX_Enumerates Enumerates
  @{
  */
/**
 * @brief   W25QXX Flash Type
 */
typedef enum
{
    W25Q80_FLASH = 1U,
    W25Q16_FLASH,
    W25Q32_FLASH,
    W25Q64_FLASH,
    W25Q128_FLASH,
    W25Q256_FLASH,
} W25QXX_TYPE_T;

/**
 * @brief   W25QXX Status Register Number
 */
typedef enum
{
    W25QXX_STATUS_REG_1 = 1U,
    W25QXX_STATUS_REG_2 = 2U,
    W25QXX_STATUS_REG_3 = 3U,
} W25QXX_STA_REG_NUM_T;
/**@} end of group BSP_W25QXX_Enumerates*/

/** @defgroup BSP_W25QXX_Structures Structures
  @{
  */
/**
 * @brief   W25QXX Information
 */
typedef struct
{
    uint8_t     type;
    uint16_t    flashID;
    uint32_t    flashSize;
    uint32_t    sectorSize;
    uint32_t    sectorNum;
    uint8_t     writeBuffer[W25QXX_SECTOR_SIZE];
} W25QXX_INFO_T;

/**@} end of group BSP_W25QXX_Structures*/

/** @defgroup BSP_W25QXX_Variables Variables
  @{
  */
extern W25QXX_INFO_T w25qxxInfo;
/**@} end of group BSP_W25QXX_Variables*/

/** @defgroup BSP_W25QXX_Functions
  @{
*/
void W25QXX_Init(W25QXX_TYPE_T type);
uint16_t W25QXX_ReadFlashID(void);
void W25QXX_ReadData(uint8_t* buffer,uint32_t addr,uint16_t length);
void W25QXX_WriteData(uint8_t* buffer,uint32_t addr,uint16_t length);
void W25QXX_EraseChip(void);
void W25QXX_EraseSector(uint32_t addr);
void W25QXX_PowerDown(void);
void W25QXX_Wakeup(void);
/**@} end of group BSP_W25QXX_Functions */
/**@} end of group Board_APM32F407_EVAL */
/**@} end of group Board */

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*__BSP_W25QXX_H */
