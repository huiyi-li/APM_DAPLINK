/*!
 * @file        bsp_sdcard.h
 *
 * @brief       Header for bsp_sdcard.c module
 *
 * @version     V1.0.1
 *
 * @date        2023-03-27
 *
 * @attention
 *
 *  Copyright (C) 2021-2023 Geehy Semiconductor
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
#ifndef __BSP_SDCARD_H
#define __BSP_SDCARD_H

#ifdef __cplusplus
  extern "C" {
#endif

/* Includes */
#include "apm32f4xx.h"
#include "apm32f4xx_misc.h"
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "apm32f4xx_sdio.h"
#include "apm32f4xx_dma.h"

/** @addtogroup Board
  @{
*/
/** @addtogroup Board_APM32F407_EVAL
  @{
*/
/** @defgroup APM32F407_EVAL_Macros Macros
  @{
*/

/* SDIO init frequency  400KHz */
#define SDIO_INIT_CLK_DIV                           0x76

#define SD_BLOCK_SIZE                               512

#define SDIO_TRANSFER_CLK_DIV                       2

/* SDIO commands index */
#define SD_CMD_GO_IDLE_STATE                        ((uint8_t)0)
#define SD_CMD_SEND_OP_COND                         ((uint8_t)1)
#define SD_CMD_ALL_SEND_CID                         ((uint8_t)2)
#define SD_CMD_SET_REL_ADDR                         ((uint8_t)3) /*!< SDIO_SEND_REL_ADDR for SD Card */
#define SD_CMD_SET_DSR                              ((uint8_t)4)
#define SD_CMD_SDIO_SEN_OP_COND                     ((uint8_t)5)
#define SD_CMD_HS_SWITCH                            ((uint8_t)6)
#define SD_CMD_SEL_DESEL_CARD                       ((uint8_t)7)
#define SD_CMD_HS_SEND_EXT_CSD                      ((uint8_t)8)
#define SD_CMD_SEND_CSD                             ((uint8_t)9)
#define SD_CMD_SEND_CID                             ((uint8_t)10)
#define SD_CMD_READ_DAT_UNTIL_STOP                  ((uint8_t)11) /*!< SD Card doesn't support it */
#define SD_CMD_STOP_TRANSMISSION                    ((uint8_t)12)
#define SD_CMD_SEND_STATUS                          ((uint8_t)13)
#define SD_CMD_HS_BUSTEST_READ                      ((uint8_t)14)
#define SD_CMD_GO_INACTIVE_STATE                    ((uint8_t)15)
#define SD_CMD_SET_BLOCKLEN                         ((uint8_t)16)
#define SD_CMD_READ_SINGLE_BLOCK                    ((uint8_t)17)
#define SD_CMD_READ_MULT_BLOCK                      ((uint8_t)18)
#define SD_CMD_HS_BUSTEST_WRITE                     ((uint8_t)19)
#define SD_CMD_WRITE_DAT_UNTIL_STOP                 ((uint8_t)20)
#define SD_CMD_SET_BLOCK_COUNT                      ((uint8_t)23)
#define SD_CMD_WRITE_SINGLE_BLOCK                   ((uint8_t)24)
#define SD_CMD_WRITE_MULT_BLOCK                     ((uint8_t)25)
#define SD_CMD_PROG_CID                             ((uint8_t)26)
#define SD_CMD_PROG_CSD                             ((uint8_t)27)
#define SD_CMD_SET_WRITE_PROT                       ((uint8_t)28)
#define SD_CMD_CLR_WRITE_PROT                       ((uint8_t)29)
#define SD_CMD_SEND_WRITE_PROT                      ((uint8_t)30)
#define SD_CMD_SD_ERASE_GRP_START                   ((uint8_t)32) /*!< To set the address of the first write block to be erased. (For SD card only) */
#define SD_CMD_SD_ERASE_GRP_END                     ((uint8_t)33) /**
                                                                  *    To set the address of the last write block of the
                                                                  *    continuous range to be erased. (For SD card only)
                                                                  */
#define SD_CMD_ERASE_GRP_START                      ((uint8_t)35) /**
                                                                  *    To set the address of the first write block to be erased.
                                                                  *    (For MMC card only spec 3.31)
                                                                  */
#define SD_CMD_ERASE_GRP_END                        ((uint8_t)36) /**
                                                                  *    To set the address of the last write block of the
                                                                  *    continuous range to be erased. (For MMC card only spec 3.31)
                                                                  */
#define SD_CMD_ERASE                                ((uint8_t)38)
#define SD_CMD_FAST_IO                              ((uint8_t)39) /*!< SD Card doesn't support it */
#define SD_CMD_GO_IRQ_STATE                         ((uint8_t)40) /*!< SD Card doesn't support it */
#define SD_CMD_LOCK_UNLOCK                          ((uint8_t)42)
#define SD_CMD_APP_CMD                              ((uint8_t)55)
#define SD_CMD_GEN_CMD                              ((uint8_t)56)
#define SD_CMD_NO_CMD                               ((uint8_t)64)

/*!
 * @brief    Following commands are SD card specific commands.
 *           SDIO_APP_CMD CMD55 should be sent before sending these commands.
 */
#define SD_CMD_APP_SD_SET_BUSWIDTH                  ((uint8_t)6)  /*!< For SD Card only */
#define SD_CMD_SD_APP_STAUS                         ((uint8_t)13) /*!< For SD Card only */
#define SD_CMD_SD_APP_SEND_NUM_WRITE_BLOCKS         ((uint8_t)22) /*!< For SD Card only */
#define SD_CMD_SD_APP_OP_COND                       ((uint8_t)41) /*!< For SD Card only */
#define SD_CMD_SD_APP_SET_CLR_CARD_DETECT           ((uint8_t)42) /*!< For SD Card only */
#define SD_CMD_SD_APP_SEND_SCR                      ((uint8_t)51) /*!< For SD Card only */
#define SD_CMD_SDIO_RW_DIRECT                       ((uint8_t)52) /*!< For SD I/O Card only */
#define SD_CMD_SDIO_RW_EXTENDED                     ((uint8_t)53) /*!< For SD I/O Card only */

/*!
 * @brief    Following commands are SD Card Specific security commands.
 *           SDIO_APP_CMD should be sent before sending these commands.
 */
#define SD_CMD_SD_APP_GET_MKB                       ((uint8_t)43) /*!< For SD Card only */
#define SD_CMD_SD_APP_GET_MID                       ((uint8_t)44) /*!< For SD Card only */
#define SD_CMD_SD_APP_SET_CER_RN1                   ((uint8_t)45) /*!< For SD Card only */
#define SD_CMD_SD_APP_GET_CER_RN2                   ((uint8_t)46) /*!< For SD Card only */
#define SD_CMD_SD_APP_SET_CER_RES2                  ((uint8_t)47) /*!< For SD Card only */
#define SD_CMD_SD_APP_GET_CER_RES1                  ((uint8_t)48) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_READ_MULTIPLE_BLOCK    ((uint8_t)18) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_WRITE_MULTIPLE_BLOCK   ((uint8_t)25) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_ERASE                  ((uint8_t)38) /*!< For SD Card only */
#define SD_CMD_SD_APP_CHANGE_SECURE_AREA            ((uint8_t)49) /*!< For SD Card only */
#define SD_CMD_SD_APP_SECURE_WRITE_MKB              ((uint8_t)48) /*!< For SD Card only */

/* Supported SD memory cards  */
#define SD_STD_CAPACITY_SD_CARD_V1_1                ((uint32_t)0x00000000)
#define SD_STD_CAPACITY_SD_CARD_V2_0                ((uint32_t)0x00000001)

#define SD_STD_CAPACITY_SD_CARD                     ((uint32_t)0x00000000)
#define SD_HIGH_CAPACITY_SD_CARD                    ((uint32_t)0x00000001)
#define SD_SECURE_DIGITAL_IO_CARD                   ((uint32_t)0x00000003)

/* SDIO card parmmeter */
#define SDIO_CMDTIMEOUT                             ((uint32_t)0x0002000)
#define SDIO_STATIC_FLAGS                           ((uint32_t)(SDIO_FLAG_COMRESP | SDIO_FLAG_DBDR | SDIO_FLAG_CMDRESTO | \
                                                                SDIO_FLAG_DATATO | SDIO_FLAG_TXUDRER | SDIO_FLAG_RXOVRER | \
                                                                SDIO_FLAG_CMDRES | SDIO_FLAG_CMDSENT | SDIO_FLAG_DATAEND | \
                                                                SDIO_FLAG_DBCP | SDIO_FLAG_SDIOINT))

/* Mask for errors Card Status R1 (OCR Register) */
#define SD_OCR_ADDR_OUT_OF_RANGE                    ((uint32_t)0x80000000)
#define SD_OCR_ADDR_MISALIGNED                      ((uint32_t)0x40000000)
#define SD_OCR_BLOCK_LEN_ERR                        ((uint32_t)0x20000000)
#define SD_OCR_ERASE_SEQ_ERR                        ((uint32_t)0x10000000)
#define SD_OCR_BAD_ERASE_PARAM                      ((uint32_t)0x08000000)
#define SD_OCR_WRITE_PROT_VIOLATION                 ((uint32_t)0x04000000)
#define SD_OCR_LOCK_UNLOCK_FAILED                   ((uint32_t)0x01000000)
#define SD_OCR_COM_CRC_FAILED                       ((uint32_t)0x00800000)
#define SD_OCR_ILLEGAL_CMD                          ((uint32_t)0x00400000)
#define SD_OCR_CARD_ECC_FAILED                      ((uint32_t)0x00200000)
#define SD_OCR_CC_ERROR                             ((uint32_t)0x00100000)
#define SD_OCR_GENERAL_UNKNOWN_ERROR                ((uint32_t)0x00080000)
#define SD_OCR_STREAM_READ_UNDERRUN                 ((uint32_t)0x00040000)
#define SD_OCR_STREAM_WRITE_OVERRUN                 ((uint32_t)0x00020000)
#define SD_OCR_CID_CSD_OVERWRIETE                   ((uint32_t)0x00010000)
#define SD_OCR_WP_ERASE_SKIP                        ((uint32_t)0x00008000)
#define SD_OCR_CARD_ECC_DISABLED                    ((uint32_t)0x00004000)
#define SD_OCR_ERASE_RESET                          ((uint32_t)0x00002000)
#define SD_OCR_AKE_SEQ_ERROR                        ((uint32_t)0x00000008)
#define SD_OCR_ERRORBITS                            ((uint32_t)0xFDFFE008)

/* Masks for R6 Response */
#define SD_R6_GENERAL_UNKNOWN_ERROR                 ((uint32_t)0x00002000)
#define SD_R6_ILLEGAL_CMD                           ((uint32_t)0x00004000)
#define SD_R6_COM_CRC_FAILED                        ((uint32_t)0x00008000)

#define SD_VOLTAGE_WINDOW_SD                        ((uint32_t)0x80100000)
#define SD_HIGH_CAPACITY                            ((uint32_t)0x40000000)
#define SD_STD_CAPACITY                             ((uint32_t)0x00000000)
#define SD_CHECK_PATTERN                            ((uint32_t)0x000001AA)
#define SD_VOLTAGE_WINDOW_MMC                       ((uint32_t)0x80FF8000)
#define SD_SWITCH_1_8V_CAPACITY                     ((uint32_t)0x01000000)

#define SD_MAX_VOLT_TRIAL                           ((uint32_t)0x0000FFFF)
#define SD_ALLZERO                                  ((uint32_t)0x00000000)

#define SD_WIDE_BUS_SUPPORT                         ((uint32_t)0x00040000)
#define SD_SINGLE_BUS_SUPPORT                       ((uint32_t)0x00010000)
#define SD_CARD_LOCKED                              ((uint32_t)0x02000000)
#define SD_CARD_PROGRAMMING                         ((uint32_t)0x00000007)
#define SD_CARD_RECEIVING                           ((uint32_t)0x00000006)
#define SD_DATATIMEOUT                              ((uint32_t)0xFFFFFFFF)
#define SD_0TO7BITS                                 ((uint32_t)0x000000FF)
#define SD_8TO15BITS                                ((uint32_t)0x0000FF00)
#define SD_16TO23BITS                               ((uint32_t)0x00FF0000)
#define SD_24TO31BITS                               ((uint32_t)0xFF000000)
#define SD_MAX_DATA_LENGTH                          ((uint32_t)0x01FFFFFF)

#define SD_HALFFIFO                                 ((uint32_t)0x00000008)
#define SD_HALFFIFOBYTES                            ((uint32_t)0x00000020)

/* Command Class Supported */
#define SD_CCCC_LOCK_UNLOCK                         ((uint32_t)0x00000080)
#define SD_CCCC_WRITE_PROT                          ((uint32_t)0x00000040)
#define SD_CCCC_ERASE                               ((uint32_t)0x00000020)

/* Command 8 */
#define SDIO_SEND_IF_COND                           ((uint32_t)0x00000008)

/* SD Card Status */
#define SD_CARD_READY                               ((uint32_t)0x00000001)
#define SD_CARD_TRANSFER                            ((uint32_t)0x00000004)


/**@} end of group APM32F407_EVAL_Macros*/

/** @defgroup APM32F407_EVAL_Enumerates Enumerates
  @{
  */

/**
 * @brief    SD status
 */
typedef enum
{
    SD_OK,
    SD_BUSY,
    SD_ERR,
    SD_TIMEOUT,
} SD_STATUS_T;

/*!
 * @brief    SD error enumeration
 */
typedef enum
{
    SD_ERR_NONE,
    SD_ERR_CMD_CRC_FAIL,        /*!< Command response received (but CRC check failed) */
    SD_ERR_CMD_RSP_TIMEOUT,     /*!< Command response timeout */
    SD_ADDR_OUT_OF_RANGE,
    SD_ADDR_MIS_ALIGNED,
    SD_BLOCK_LEN_ERR,
    SD_ERASE_SEQ_ERR,
    SD_BAD_ERASE_PARAM,
    SD_WRITE_PROT_VIOLATION,
    SD_LOCK_UNLOCK_FAILED,
    SD_COM_CRC_FAILED,
    SD_ILLEGAL_CMD,
    SD_CARD_ECC_FAILED,
    SD_CC_ERROR,
    SD_STREAM_READ_UNDERRUN,
    SD_STREAM_WRITE_OVERRUN,
    SD_CID_CSD_OVERWRITE,
    SD_WP_ERASE_SKIP,
    SD_CARD_ECC_DISABLED,
    SD_ERASE_RESET,
    SD_AKE_SEQ_ERROR,
    SD_UNSUPPORTED_FEATURE,
    SD_INVALID_VOLTRANGE,
    SD_REQUEST_NOT_APPLICABLE,
    SD_GENERAL_UNKNOWN_ERROR,
    SD_INVALID_PARAMETER,
    SD_DATA_TIMEOUT,
    SD_DATA_CRC_FAIL,
    SD_RX_OVERRUN,
    SD_TX_UNDERRUN,
} SD_ERR_T;

/*!
 * @brief    SD state
 */
typedef enum
{
    SD_STATE_IDLE,
    SD_STATE_BUSY,
    SD_STATE_READY,
    SD_STATE_TIMEOUT,
    SD_STATE_ERROR,
} SD_STATE_T;

/**@} end of group APM32F407_EVAL_Enumerates*/

/** @defgroup APM32F407_EVAL_Structures Structures
  @{
  */

/*!
 * @brief    SD Card Information
 */
typedef struct
{
    SD_STATE_T      state;
    uint32_t        cardType;
    uint32_t        cardVer;
    uint32_t        cardClass;
    uint32_t        cid[4];
    uint32_t        csd[4];
    uint16_t        rca;
    
    uint32_t        blockNum;
    uint32_t        blockSize;
    uint32_t        logBlockNum;
    uint32_t        logBlockSize;
    
} SD_CARD_INFO_T;

extern SD_CARD_INFO_T sdmmcInfo;
/**@} end of group APM32F407_EVAL_Structures*/

/** @defgroup APM32F407_EVAL_Functions Functions
  @{
  */
uint32_t SD_ReadCardState(SD_CARD_INFO_T *sdInfo);
uint32_t SD_ReadState(SD_CARD_INFO_T *sdInfo);
SD_STATUS_T SD_Init(SD_CARD_INFO_T *sdInfo);
void SD_ShowCardInfo(SD_CARD_INFO_T *sdInfo);
void SD_ResetSDIOReg(void);
SD_STATUS_T SD_ReadBlocks(SD_CARD_INFO_T *sdInfo, uint8_t *data, uint32_t blkAddr, uint32_t blkNum, uint32_t timeout);
SD_STATUS_T SD_WriteBlocks(SD_CARD_INFO_T *sdInfo, uint8_t *data, uint32_t blkAddr, uint32_t blkNum, uint32_t timeout);
/**@} end of group APM32F407_EVAL_Functions */
/**@} end of group APM32F407_EVAL */
/**@} end of group Examples */

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SDCARD_H */
