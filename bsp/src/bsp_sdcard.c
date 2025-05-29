/*!
 * @file        bsp_sdcard.c
 *
 * @brief       SD Card board support package body
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

/* Includes */
#include "bsp_sdcard.h"
#include "bsp_delay.h"
#include <stdio.h>
#include <string.h>

/** @addtogroup Board
  @{
*/
/** @addtogroup Board_APM32F407_EVAL
  @{
*/
/** @defgroup APM32F407_EVAL_Variables
  @{
*/

/**@} end of group APM32F407_EVAL_Variables */

/** @defgroup APM32F407_EVAL_Structures Structures
  @{
  */
SD_CARD_INFO_T sdmmcInfo;
/**@} end of group APM32F407_EVAL_Structures*/

/** @defgroup APM32F407_EVAL_Functions
  @{
*/

/*!
 * @brief       SDIO Init
 *
 * @param       None
 *
 * @retval      None
 *
 */
static void SDIO_Init(void)
{
    GPIO_Config_T GPIO_InitStructure;
    
    /* Enable the GPIO Clock */
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOC | \
                              RCM_AHB1_PERIPH_GPIOD);

    /* Enable the SDIO Clock */
    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SDIO);
    
    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SYSCFG);

    /* GPIO PIN AF */
    GPIO_ConfigPinAF(GPIOC,GPIO_PIN_SOURCE_8,GPIO_AF_SDIO);
    GPIO_ConfigPinAF(GPIOC,GPIO_PIN_SOURCE_9,GPIO_AF_SDIO);
    GPIO_ConfigPinAF(GPIOC,GPIO_PIN_SOURCE_10,GPIO_AF_SDIO);
    GPIO_ConfigPinAF(GPIOC,GPIO_PIN_SOURCE_11,GPIO_AF_SDIO);
    GPIO_ConfigPinAF(GPIOC,GPIO_PIN_SOURCE_12,GPIO_AF_SDIO);
    GPIO_ConfigPinAF(GPIOD,GPIO_PIN_SOURCE_2,GPIO_AF_SDIO);
    
    /* Configure the GPIO pin */
    GPIO_InitStructure.pin      = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | \
                                  GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStructure.mode     = GPIO_MODE_AF;
    GPIO_InitStructure.speed    = GPIO_SPEED_100MHz;
    GPIO_InitStructure.otype    = GPIO_OTYPE_PP;
    GPIO_InitStructure.pupd     = GPIO_PUPD_UP;
    GPIO_Config(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.pin      = GPIO_PIN_2;
    GPIO_Config(GPIOD, &GPIO_InitStructure);
}

/*!
 * @brief        Check response of CMD0
 *
 * @param        None
 *
 * @retval       SD error code
 *
 */
static SD_ERR_T SD_CheckCmd0Error(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    uint32_t timeout = SDIO_CMDTIMEOUT * (SystemCoreClock / 1000);

    while(SDIO_ReadStatusFlag(SDIO_FLAG_CMDSENT) == RESET)
    {
        if(timeout-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    /* Clear interrupt flag */
    SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);

    return errorStatus;
}

/*!
 * @brief        Check R1 response of current command
 *
 * @param        cmd: command
 *
 * @param        timeout
 *
 * @retval       SD error code
 *
 */
static SD_ERR_T SD_CmdResp1Error(uint8_t cmd, uint32_t timeout)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    uint32_t status;
    uint32_t response;

    uint32_t cnt = timeout * (SystemCoreClock / 8 / 1000);
    
    status = SDIO->STS;
    while(((status & (SDIO_FLAG_COMRESP | SDIO_FLAG_CMDRES | SDIO_FLAG_CMDRESTO)) == 0) || \
          ((status & SDIO_FLAG_CMDACT) != 0))
    {
        status = SDIO->STS;
        
        if(cnt-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    if((SDIO->STS & SDIO_FLAG_CMDRESTO) != 0)
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_CMDRESTO);
        return SD_ERR_CMD_RSP_TIMEOUT;
    }
    else if(SDIO_ReadStatusFlag(SDIO_FLAG_COMRESP))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_COMRESP);
        return SD_ERR_CMD_CRC_FAIL;
    }
    
    /* Clear all the static flags */
    SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
    
    if(SDIO_ReadCommandResponse() != cmd)
    {
        return SD_ERR_CMD_CRC_FAIL;
    }
    
    response = SDIO_ReadResponse(SDIO_RES1);
    
    if((response & SD_OCR_ERRORBITS) == SD_ALLZERO)
    {
        errorStatus = SD_ERR_NONE;
    }
    else if((response & SD_OCR_ADDR_OUT_OF_RANGE) == SD_OCR_ADDR_OUT_OF_RANGE)
    {
        errorStatus = SD_ADDR_OUT_OF_RANGE;
    }
    else if((response & SD_OCR_ADDR_MISALIGNED) == SD_OCR_ADDR_MISALIGNED)
    {
        errorStatus = SD_ADDR_MIS_ALIGNED;
    }
    else if((response & SD_OCR_BLOCK_LEN_ERR) == SD_OCR_BLOCK_LEN_ERR)
    {
        errorStatus = SD_BLOCK_LEN_ERR;
    }
    else if((response & SD_OCR_ERASE_SEQ_ERR) == SD_OCR_ERASE_SEQ_ERR)
    {
        errorStatus = SD_ERASE_SEQ_ERR;
    }
    else if((response & SD_OCR_BAD_ERASE_PARAM) == SD_OCR_BAD_ERASE_PARAM)
    {
        errorStatus = SD_BAD_ERASE_PARAM;
    }
    else if((response & SD_OCR_WRITE_PROT_VIOLATION) == SD_OCR_WRITE_PROT_VIOLATION)
    {
        errorStatus = SD_WRITE_PROT_VIOLATION;
    }
    else if((response & SD_OCR_LOCK_UNLOCK_FAILED) == SD_OCR_LOCK_UNLOCK_FAILED)
    {
        errorStatus = SD_LOCK_UNLOCK_FAILED;
    }
    else if((response & SD_OCR_COM_CRC_FAILED) == SD_OCR_COM_CRC_FAILED)
    {
        errorStatus = SD_COM_CRC_FAILED;
    }
    else if((response & SD_OCR_ILLEGAL_CMD) == SD_OCR_ILLEGAL_CMD)
    {
        errorStatus = SD_ILLEGAL_CMD;
    }
    else if((response & SD_OCR_CARD_ECC_FAILED) == SD_OCR_CARD_ECC_FAILED)
    {
        errorStatus = SD_CARD_ECC_FAILED;
    }
    else if((response & SD_OCR_CC_ERROR) == SD_OCR_CC_ERROR)
    {
        errorStatus = SD_CC_ERROR;
    }
    else if((response & SD_OCR_STREAM_READ_UNDERRUN) == SD_OCR_STREAM_READ_UNDERRUN)
    {
        errorStatus = SD_STREAM_READ_UNDERRUN;
    }
    else if((response & SD_OCR_STREAM_WRITE_OVERRUN) == SD_OCR_STREAM_WRITE_OVERRUN)
    {
        errorStatus = SD_STREAM_WRITE_OVERRUN;
    }
    else if((response & SD_OCR_CID_CSD_OVERWRIETE) == SD_OCR_CID_CSD_OVERWRIETE)
    {
        errorStatus = SD_CID_CSD_OVERWRITE;
    }
    else if((response & SD_OCR_WP_ERASE_SKIP) == SD_OCR_WP_ERASE_SKIP)
    {
        errorStatus = SD_WP_ERASE_SKIP;
    }
    else if((response & SD_OCR_CARD_ECC_DISABLED) == SD_OCR_CARD_ECC_DISABLED)
    {
        errorStatus = SD_CARD_ECC_DISABLED;
    }
    else if((response & SD_OCR_ERASE_RESET) == SD_OCR_ERASE_RESET)
    {
        errorStatus = SD_ERASE_RESET;
    }
    else if((response & SD_OCR_AKE_SEQ_ERROR) == SD_OCR_AKE_SEQ_ERROR)
    {
        errorStatus = SD_AKE_SEQ_ERROR;
    }

    return errorStatus;
}

/*!
 * @brief        Check R2 response
 *
 * @param        None
 *
 * @retval       SD error code
 *
 */
SD_ERR_T SD_CmdResp2Error(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    uint32_t status;
    uint32_t timeout = SDIO_CMDTIMEOUT * (SystemCoreClock / 1000);

    status = SDIO->STS;
    while(((status & (SDIO_FLAG_COMRESP | SDIO_FLAG_CMDRES | SDIO_FLAG_CMDRESTO)) == 0) || \
          ((status & SDIO_FLAG_CMDACT) != 0))
    {
        status = SDIO->STS;
        
        if(timeout-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    /* Response timeout */
    if(SDIO_ReadStatusFlag(SDIO_FLAG_CMDRESTO))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_CMDRESTO);
        return SD_ERR_CMD_RSP_TIMEOUT;
    }
    else if(SDIO_ReadStatusFlag(SDIO_FLAG_COMRESP))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_COMRESP);
        return SD_ERR_CMD_CRC_FAIL;
    }
    else
    {
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
    }

    return errorStatus;
}

/*!
 * @brief        Check R3 response
 *
 * @param        None
 *
 * @retval       SD error code
 *
 */
SD_ERR_T SD_CmdResp3Error(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    uint32_t status;

    uint32_t timeout = SDIO_CMDTIMEOUT * (SystemCoreClock / 1000);

    status = SDIO->STS;
    while(((status & (SDIO_FLAG_COMRESP | SDIO_FLAG_CMDRES | SDIO_FLAG_CMDRESTO)) == 0) || \
          ((status & SDIO_FLAG_CMDACT) != 0))
    {
        status = SDIO->STS;
        
        if(timeout-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    if(SDIO_ReadStatusFlag(SDIO_FLAG_CMDRESTO) != RESET)
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_CMDRESTO);
        return SD_ERR_CMD_RSP_TIMEOUT;
    }
    else
    {
      SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
    }

    return errorStatus;
}

/*!
 * @brief        Check R6 response of current command
 *
 * @param        cmd: command
 *
 * @param        prca: RCA value
 *
 * @retval       SD error code
 *
 */
SD_ERR_T SD_CmdResp6Error(uint8_t cmd,uint16_t *prca)
{
    uint32_t status;
    uint32_t response;

    uint32_t timeout = SDIO_CMDTIMEOUT * (SystemCoreClock / 1000);
    
    status = SDIO->STS;
    while(((status & (SDIO_FLAG_COMRESP | SDIO_FLAG_CMDRES | SDIO_FLAG_CMDRESTO)) == 0) || \
          ((status & SDIO_FLAG_CMDACT) != 0))
    {
        status = SDIO->STS;
        
        if(timeout-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    if(SDIO_ReadStatusFlag(SDIO_FLAG_CMDRESTO))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_CMDRESTO);
        return SD_ERR_CMD_RSP_TIMEOUT;
    }
    else if(SDIO_ReadStatusFlag(SDIO_FLAG_COMRESP))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_COMRESP);
        return SD_ERR_CMD_CRC_FAIL;
    }
    
    if(SDIO_ReadCommandResponse() != cmd)
    {
        return SD_ERR_CMD_CRC_FAIL;
    }

    SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
    
    /* Get response */
    response = SDIO_ReadResponse(SDIO_RES1);

    if((response & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED)) == SD_ALLZERO)
    {
        *prca = (uint16_t)(response >> 16);
        
        return SD_ERR_NONE;
    }
    else if(response & SD_R6_ILLEGAL_CMD)
    {
        return SD_ILLEGAL_CMD;
    }
    if(response & SD_R6_COM_CRC_FAILED)
    {
        return SD_COM_CRC_FAILED;
    }
    else
    {
        return SD_GENERAL_UNKNOWN_ERROR;
    }
}

/*!
 * @brief        Check R7 response
 *
 * @param        None
 *
 * @retval       SD error code
 *
 */
static SD_ERR_T SD_CmdResp7Error(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    uint32_t status;
    uint32_t timeout = SDIO_CMDTIMEOUT * (SystemCoreClock / 1000);

    status = SDIO->STS;
    while(((status & (SDIO_FLAG_COMRESP | SDIO_FLAG_CMDRES | SDIO_FLAG_CMDRESTO)) == 0) || \
          ((status & SDIO_FLAG_CMDACT) != 0))
    {
        status = SDIO->STS;
        
        if(timeout-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    /* Response timeout */
    if(SDIO_ReadStatusFlag(SDIO_FLAG_CMDRESTO))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_CMDRESTO);
        
        return SD_ERR_CMD_RSP_TIMEOUT;
    }
    else if(SDIO_ReadStatusFlag(SDIO_FLAG_COMRESP))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_COMRESP);
        
        return SD_ERR_CMD_CRC_FAIL;
    }
    
    
    /* Receive response */
    if(SDIO_ReadStatusFlag(SDIO_FLAG_CMDRES))
    {
        errorStatus = SD_ERR_NONE;
        SDIO_ClearStatusFlag(SDIO_FLAG_CMDRES);
    }

    return errorStatus;
}

/*!
 * @brief       Send Go to Idle State Command
 *
 * @param       None
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdGoIdleState(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = 0x0;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_GO_IDLE_STATE;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_NO;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the response */
    errorStatus = SD_CheckCmd0Error();
    
    return errorStatus;
}

/*!
 * @brief       Send Operation Condition Command
 *
 * @param       None
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdIfCondition(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = SD_CHECK_PATTERN;
    sdioCmdConfigStructure.cmdIndex = SDIO_SEND_IF_COND;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R7 response */
    errorStatus = SD_CmdResp7Error();
    
    return errorStatus;
}

/*!
 * @brief       Send Application Command
 *
 * @param       arg
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdApp(uint32_t arg)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = arg;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_APP_CMD;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_APP_CMD, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Application Operation Command
 *
 * @param       arg
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdAppOp(uint32_t arg)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = SD_VOLTAGE_WINDOW_SD | arg;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SD_APP_OP_COND;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R3 response */
    errorStatus = SD_CmdResp3Error();
    
    return errorStatus;
}

/*!
 * @brief       Send CID Command
 *
 * @param       None
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdCID(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = 0;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_ALL_SEND_CID;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_LONG;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R2 response */
    errorStatus = SD_CmdResp2Error();
    
    return errorStatus;
}

/*!
 * @brief       Send RCA Command
 *
 * @param       rca
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdRCA(uint16_t *rca)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = 0;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SET_REL_ADDR;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R6 response */
    errorStatus = SD_CmdResp6Error(SD_CMD_SET_REL_ADDR, rca);
    
    return errorStatus;
}

/*!
 * @brief       Send CSD Command
 *
 * @param       rca
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdCSD(uint32_t arg)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = arg;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SEND_CSD;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_LONG;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R2 response */
    errorStatus = SD_CmdResp2Error();
    
    return errorStatus;
}

/*!
 * @brief       Send Select or Deselect Command
 *
 * @param       addr
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdSelDel(uint32_t addr)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = addr;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SEL_DESEL_CARD;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_SEL_DESEL_CARD, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Block length Command
 *
 * @param       size
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdBlockLength(uint32_t size)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = size;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SET_BLOCKLEN;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_SET_BLOCKLEN, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Bus Wide Command
 *
 * @param       busWidth
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdBusWide(uint32_t busWidth)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = busWidth;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_APP_SD_SET_BUSWIDTH;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send SCR Command
 *
 * @param       None
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdSCR(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = 0;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SD_APP_SEND_SCR;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_SD_APP_SEND_SCR, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Read Multi Block Command
 *
 * @param       addr
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdReadMultiBlock(uint32_t addr)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = addr;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_READ_MULT_BLOCK;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_READ_MULT_BLOCK, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Read Single Block Command
 *
 * @param       addr
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdReadSingleBlock(uint32_t addr)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = addr;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_READ_SINGLE_BLOCK;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Write Multi Block Command
 *
 * @param       addr
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdWriteMultiBlock(uint32_t addr)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = addr;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_WRITE_MULT_BLOCK;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Write Single Block Command
 *
 * @param       addr
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdWriteSingleBlock(uint32_t addr)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = addr;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_WRITE_SINGLE_BLOCK;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Send Status Command
 *
 * @param       arg
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdSendStatus(uint32_t arg)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = arg;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_SEND_STATUS;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_SEND_STATUS, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Send Stop Transfer Command
 *
 * @param       None
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_SendCmdStopTransfer(void)
{
    SD_ERR_T errorStatus = SD_ERR_NONE;
    
    SDIO_CmdConfig_T sdioCmdConfigStructure;
    
    sdioCmdConfigStructure.argument = 0;
    sdioCmdConfigStructure.cmdIndex = SD_CMD_STOP_TRANSMISSION;
    sdioCmdConfigStructure.response = SDIO_RESPONSE_SHORT;
    sdioCmdConfigStructure.wait     = SDIO_WAIT_NO;
    sdioCmdConfigStructure.CPSM     = SDIO_CPSM_ENABLE;
    SDIO_TxCommand(&sdioCmdConfigStructure);
    
    /* Wait the R1 response */
    errorStatus = SD_CmdResp1Error(SD_CMD_STOP_TRANSMISSION, SDIO_CMDTIMEOUT);
    
    return errorStatus;
}

/*!
 * @brief       Power on SD card. Query all card device from SDIO interface.
 *              And check voltage value and set clock.
 *
 * @param       sdInfo
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_PowerON(SD_CARD_INFO_T *sdInfo)
{
    SD_ERR_T errStatus = SD_ERR_NONE;
    uint32_t response = 0;
    uint32_t count = 0;
    uint32_t validVoltage = 0;
    
    /* Send CMD0 to goto idle stage */
    errStatus = SD_SendCmdGoIdleState();
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    /* Send CMD8 to check SD card interface characteristic */
    errStatus = SD_SendCmdIfCondition();
    if(errStatus != SD_ERR_NONE)
    {
        sdInfo->cardVer = SD_STD_CAPACITY_SD_CARD_V1_1;
        
        /* Send CMD0 to goto idle stage */
        errStatus = SD_SendCmdGoIdleState();
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
    }
    else
    {
        sdInfo->cardVer = SD_STD_CAPACITY_SD_CARD_V2_0;
    }
    
    if(sdInfo->cardVer == SD_STD_CAPACITY_SD_CARD_V2_0)
    {
        /* Send CMD55 command before ACMD command */
        errStatus = SD_SendCmdApp(0);
        if(errStatus != SD_ERR_NONE)
        {
            return SD_UNSUPPORTED_FEATURE;
        }
    }
    
    /* Send CMD55 + ACMD41 */
    while((validVoltage == 0) && (count < SD_MAX_VOLT_TRIAL))
    {
        /* Send CMD55 command before ACMD command */
        errStatus = SD_SendCmdApp(0);
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        
        /**
        *    Send ACMD41 command.
        *    The command argument consists of the supported voltage range and HCS position.
        *    The HCS position can be use to judge SD card type(SDSC or SDHC).
        */
        errStatus = SD_SendCmdAppOp(SD_VOLTAGE_WINDOW_SD | SD_HIGH_CAPACITY | SD_SWITCH_1_8V_CAPACITY);
        if(errStatus != SD_ERR_NONE)
        {
            return SD_UNSUPPORTED_FEATURE;
        }
        
        /* Get the response */
        response = SDIO_ReadResponse(SDIO_RES1);
        
        /* Judge the power on finish or not */
        validVoltage = (((response >> 31) == 1) ? 1 : 0);

        count++;
    }
    
    if(count >= SD_MAX_VOLT_TRIAL)
    {
        /* Exceed voltage range */
        return SD_INVALID_VOLTRANGE;
    }
    
    if((response & SD_HIGH_CAPACITY) == SD_HIGH_CAPACITY)
    {
        sdInfo->cardType = SD_HIGH_CAPACITY_SD_CARD;
    }
    else
    {
        sdInfo->cardType = SD_STD_CAPACITY_SD_CARD;
    }
    
    return errStatus;
}

/*!
 * @brief       Get SD card informations
 *
 * @param       sdInfo
 *
 * @retval      SD error code
 *
 */
static SD_STATUS_T SD_GetCardInfo(SD_CARD_INFO_T *sdInfo)
{
    SD_STATUS_T status = SD_OK;
    uint32_t temp;
    
    if(sdInfo->cardType == SD_STD_CAPACITY_SD_CARD)
    {
        temp = (uint8_t)((sdInfo->csd[2] & 0x00038000) >> 15);
        sdInfo->blockNum = ((((sdInfo->csd[1] & 0x000003FF) << 2) | ((sdInfo->csd[2] & 0xC0000000) >> 30)) + 1);
        sdInfo->blockNum *= (1 << ((temp & 0x07) + 2));
        
        temp = (uint8_t)((sdInfo->csd[1] & 0x000F0000) >> 16);
        sdInfo->blockSize = (1 << (temp & 0x0F));
        
        sdInfo->logBlockNum = (sdInfo->blockNum) * ((sdInfo->blockSize) / SD_BLOCK_SIZE);
        sdInfo->logBlockSize = SD_BLOCK_SIZE;
    }
    else if(sdInfo->cardType == SD_HIGH_CAPACITY_SD_CARD)
    {
        temp = (((sdInfo->csd[1] & 0x0000003F) << 16) | ((sdInfo->csd[2] & 0xFFFF0000) >> 16));
        sdInfo->blockNum = ((temp + 1) * 1024);
        sdInfo->blockSize = SD_BLOCK_SIZE;
        
        sdInfo->logBlockNum = sdInfo->blockNum;
        sdInfo->logBlockSize = sdInfo->blockSize;
    }
    else
    {
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        
        sdInfo->state = SD_STATE_READY;
        
        return SD_ERR;
    }
    
    return status;
}

/*!
 * @brief       Initialize SD card and let it into ready status
 *
 * @param       sdInfo
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_InitializeCard(SD_CARD_INFO_T *sdInfo)
{
    SD_ERR_T errStatus = SD_ERR_NONE;
    SD_STATUS_T status = SD_OK;
    
    uint16_t rca = 0x01;
    
    /* Check the power state of SD card */
    if(SDIO_ReadPowerState() == SDIO_POWER_STATE_OFF)
    {
        return SD_REQUEST_NOT_APPLICABLE;
    }
    
    /* If the card is not secure digital io card */
    if(sdInfo->cardType != SD_SECURE_DIGITAL_IO_CARD)
    {
        /* Send CMD2 */
        errStatus = SD_SendCmdCID();
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        else
        {
            /* Get CID */
            sdInfo->cid[0] = SDIO_ReadResponse(SDIO_RES1);
            sdInfo->cid[1] = SDIO_ReadResponse(SDIO_RES2);
            sdInfo->cid[2] = SDIO_ReadResponse(SDIO_RES3);
            sdInfo->cid[3] = SDIO_ReadResponse(SDIO_RES4);
        }
        
        /* Send CMD3 */
        errStatus = SD_SendCmdRCA(&rca);
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        
        /* Store the SD Card RCA */
        sdInfo->rca = rca;
        
        /* Send CMD9 + rca */
        errStatus = SD_SendCmdCSD((uint32_t)(sdInfo->rca << 16));
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        else
        {
            /* Get CSD */
            sdInfo->csd[0] = SDIO_ReadResponse(SDIO_RES1);
            sdInfo->csd[1] = SDIO_ReadResponse(SDIO_RES2);
            sdInfo->csd[2] = SDIO_ReadResponse(SDIO_RES3);
            sdInfo->csd[3] = SDIO_ReadResponse(SDIO_RES4);
        }
    }
    
    /* Get the Card Class */
    sdInfo->cardClass = (SDIO_ReadResponse(SDIO_RES2) >> 20);
    
    /* Get Card parameters */
    status = SD_GetCardInfo(sdInfo);
    if(status != SD_OK)
    {
        return SD_UNSUPPORTED_FEATURE;
    }
    
    /* Select and enable SD card */
    SD_SendCmdSelDel((uint32_t)(sdInfo->rca << 16));
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    return errStatus;
}

/*!
 * @brief       SDIO Card Init
 *
 * @param       sdInfo
 *
 * @retval      SD status
 *
 */
static SD_STATUS_T SD_InitCard(SD_CARD_INFO_T *sdInfo)
{
    SD_STATUS_T status = SD_OK;
    SD_ERR_T errStatus = SD_ERR_NONE;

    SDIO_Config_T sdioConfigStruct;
    
    if(sdInfo->state == SD_STATE_IDLE)
    {
        SDIO_Init();
    }
    
    sdInfo->state = SD_STATE_BUSY;
    
    /**
    The clock frequency can not be set than 400KHz in initalization
    SDIOCLK = 48MHz, SDIO_CK = SDIOCLK/(118 + 2) = 400 KHz
    */
    sdioConfigStruct.clockDiv             = SDIO_INIT_CLK_DIV;
    sdioConfigStruct.clockEdge            = SDIO_CLOCK_EDGE_RISING;
    sdioConfigStruct.clockBypass          = SDIO_CLOCK_BYPASS_DISABLE;
    sdioConfigStruct.clockPowerSave       = SDIO_CLOCK_POWER_SAVE_DISABLE;
    sdioConfigStruct.hardwareFlowControl  = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    sdioConfigStruct.busWide              = SDIO_BUS_WIDE_1B;
    SDIO_Config(&sdioConfigStruct);
    
    SDIO_DisableClock();
    
    /* Set power state to ON */
    SDIO_ConfigPowerState(SDIO_POWER_STATE_ON);
    
    SDIO_EnableClock();
    
    APM_DelayMs(10);
    
    /* power on SD card */
    errStatus = SD_PowerON(sdInfo);
    if(errStatus != SD_ERR_NONE)
    {
        sdInfo->state = SD_STATE_READY;
        return SD_ERR;
    }
    
    /* Initialize SD card */
    errStatus = SD_InitializeCard(sdInfo);
    if(errStatus != SD_ERR_NONE)
    {
        sdInfo->state = SD_STATE_READY;
        return SD_ERR;
    }
    
    /* Set Block Size for Card */
    errStatus = SD_SendCmdBlockLength(SD_BLOCK_SIZE);
    if(errStatus != SD_ERR_NONE)
    {
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        sdInfo->state = SD_STATE_READY;
        return SD_ERR;
    }
    
    sdInfo->state = SD_STATE_READY;
    
    return status;
}

/*!
 * @brief       Find SD card SCR register
 *
 * @param       sdInfo
 *
 * @param       scr: SCR buffer
 *
 * @retval      SD card state code
 *
 */
static SD_ERR_T SD_FindSCR(SD_CARD_INFO_T *sdInfo, uint32_t *scr)
{
    SD_ERR_T errStatus = SD_ERR_NONE;
    SDIO_DataConfig_T sdioDataConfigStructure;
    uint32_t tempscr[2] = {0, 0};
    uint8_t index = 0;
    uint32_t timeout = SDIO_CMDTIMEOUT * (SystemCoreClock / 1000);
    uint32_t *pscr = scr;
    
    /* Send CMD16 to set block size as 8 bytes */
    errStatus = SD_SendCmdBlockLength(8);
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    /* Send CMD55 + RCA */
    errStatus = SD_SendCmdApp((uint32_t)(sdInfo->rca << 16));
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    /* Config data format */
    sdioDataConfigStructure.dataTimeOut     = SD_DATATIMEOUT;
    sdioDataConfigStructure.dataLength      = 8;
    sdioDataConfigStructure.dataBlockSize   = SDIO_DATA_BLOCKSIZE_8B;
    sdioDataConfigStructure.transferDir     = SDIO_TRANSFER_DIR_TO_SDIO;
    sdioDataConfigStructure.transferMode    = SDIO_TRANSFER_MODE_BLOCK;
    sdioDataConfigStructure.DPSM            = SDIO_DPSM_ENABLE;
    SDIO_ConfigData(&sdioDataConfigStructure);
    
    /* Send ACMD51 */
    errStatus = SD_SendCmdSCR();
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    while(!(SDIO->STS & (SDIO_FLAG_RXOVRER | SDIO_FLAG_DBDR | SDIO_FLAG_CMDRESTO)))
    {
        if(SDIO_ReadStatusFlag(SDIO_FLAG_RXDA))
        {
            *(tempscr + index) = SDIO_ReadData();
            index++;
        }
        else if(!SDIO_ReadStatusFlag(SDIO_FLAG_RXACT))
        {
            break;
        }
        
        if(timeout-- == 0)
        {
            return SD_ERR_CMD_RSP_TIMEOUT;
        }
    }
    
    if(SDIO_ReadStatusFlag(SDIO_FLAG_DATATO))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_DATATO);
        
        return SD_DATA_TIMEOUT;
    }
    else if(SDIO_ReadStatusFlag(SDIO_FLAG_DBDR))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_DBDR);

        return SD_DATA_CRC_FAIL;
    }
    else if(SDIO_ReadStatusFlag(SDIO_FLAG_RXOVRER))
    {
        SDIO_ClearStatusFlag(SDIO_FLAG_RXOVRER);
        
        return SD_RX_OVERRUN;
    }
    else
    {
        /* Clear all flags */
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        
        /* collate the data */
        *(pscr + 1) = ((tempscr[0] & SD_0TO7BITS) << 24) | ((tempscr[0] & SD_8TO15BITS) << 8) | \
                     ((tempscr[0] & SD_16TO23BITS) >> 8)|((tempscr[0] & SD_24TO31BITS) >> 24);
        
        *(pscr) = ((tempscr[1] & SD_0TO7BITS) << 24) | ((tempscr[1] & SD_8TO15BITS) << 8) | \
                 ((tempscr[1] & SD_16TO23BITS) >> 8) | ((tempscr[1] & SD_24TO31BITS) >> 24);
    }
    
    return errStatus;
}

/*!
 * @brief       Enable SDIO bus wide mode
 *
 * @param       sdInfo
 *
 * @retval      SD error code
 *
 */
static SD_ERR_T SD_EnableWideBus(SD_CARD_INFO_T *sdInfo)
{
    SD_ERR_T errStatus = SD_ERR_NONE;
    
    uint32_t scr[2] = {0, 0};
    
    if((SDIO_ReadResponse(SDIO_RES1) & SD_CARD_LOCKED) == SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_FAILED;
    }
    
    /* Get SCR register data */
    errStatus = SD_FindSCR(sdInfo, scr);
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    /* SD card support bus wide mode */
    if((scr[1] & SD_WIDE_BUS_SUPPORT) != SD_ALLZERO)
    {
        /* Send CMD55 + RCA */
        errStatus = SD_SendCmdApp((uint32_t)(sdInfo->rca << 16));
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        
        /* Send ACMD6 */
        errStatus = SD_SendCmdBusWide(2);
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        
        errStatus = SD_ERR_NONE;
    }
    else
    {
        return SD_REQUEST_NOT_APPLICABLE;
    }
    
    return errStatus;
}

/*!
 * @brief       Disable SDIO bus wide mode
 *
 * @param       sdInfo
 *
 * @retval      SD error code
 *
 */
SD_ERR_T SD_DisableWideBus(SD_CARD_INFO_T *sdInfo)
{
    SD_ERR_T errStatus = SD_ERR_NONE;
    
    uint32_t scr[2] = {0, 0};
    
    if((SDIO_ReadResponse(SDIO_RES1) & SD_CARD_LOCKED) == SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_FAILED;
    }
    
    /* Get SCR register data */
    errStatus = SD_FindSCR(sdInfo, scr);
    if(errStatus != SD_ERR_NONE)
    {
        return errStatus;
    }
    
    /* SD card support 1 bit mode */
    if((scr[1] & SD_SINGLE_BUS_SUPPORT) != SD_ALLZERO)
    {
        /* Send CMD55 + RCA */
        errStatus = SD_SendCmdApp((uint32_t)(sdInfo->rca << 16));
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        
        /* Send ACMD6 */
        errStatus = SD_SendCmdBusWide(0);
        if(errStatus != SD_ERR_NONE)
        {
            return errStatus;
        }
        
        errStatus = SD_ERR_NONE;
    }
    else
    {
        return SD_REQUEST_NOT_APPLICABLE;
    }
    
    return errStatus;
}

/*!
 * @brief       Set SDIO bus wide
 *
 * @param       sdInfo
 *
 * @param       wideMode: bus wide
 *
 * @retval      SD status
 *
 */
SD_STATUS_T SD_EnableWideBusOperation(SD_CARD_INFO_T *sdInfo, SDIO_BUS_WIDE_T wideMode)
{
    SD_STATUS_T status = SD_OK;
    SD_ERR_T errStatus = SD_ERR_NONE;
    
    SDIO_Config_T sdioInitStructure;
    
    sdInfo->state = SD_STATE_BUSY;
    
    if(sdInfo->cardType != SD_SECURE_DIGITAL_IO_CARD)
    {
        switch(wideMode)
        {
            case SDIO_BUS_WIDE_1B:
                errStatus = SD_DisableWideBus(sdInfo);
                break;
            
            case SDIO_BUS_WIDE_4B:
                errStatus = SD_EnableWideBus(sdInfo);
                break;
            
            case SDIO_BUS_WIDE_8B:
                errStatus = SD_UNSUPPORTED_FEATURE;
                break;
            
            default:
                errStatus = SD_INVALID_PARAMETER;
                break;
        }
    }
    else
    {
        errStatus = SD_UNSUPPORTED_FEATURE;
    }
    
    if(errStatus != SD_ERR_NONE)
    {
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        sdInfo->state = SD_STATE_READY;
        status = SD_ERR;
    }
    else
    {
        sdioInitStructure.clockDiv              = SDIO_TRANSFER_CLK_DIV;
        sdioInitStructure.clockEdge             = SDIO_CLOCK_EDGE_RISING;
        sdioInitStructure.clockBypass           = SDIO_CLOCK_BYPASS_DISABLE;
        sdioInitStructure.clockPowerSave        = SDIO_CLOCK_POWER_SAVE_DISABLE;
        sdioInitStructure.hardwareFlowControl   = SDIO_HARDWARE_FLOW_CONTROL_ENABLE;
        sdioInitStructure.busWide               = wideMode;
        SDIO_Config(&sdioInitStructure);
    }
    
    /* Set Block Size for Card */
    errStatus = SD_SendCmdBlockLength(SD_BLOCK_SIZE);
    if(errStatus != SD_ERR_NONE)
    {
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        status = SD_ERR;
    }
    
    sdInfo->state = SD_STATE_READY;
    
    return status;
}

/*!
 * @brief       SD Reset SDIO Register
 *
 * @param       None
 *
 * @retval      None
 *
 */
void SD_ResetSDIOReg(void)
{
    SDIO->PWRCTRL = 0x00000000;
    SDIO->CLKCTRL = 0x00000000;
    SDIO->ARG = 0x00000000;
    SDIO->CMD = 0x00000000;
    SDIO->DATATIME = 0x00000000;
    SDIO->DATALEN = 0x00000000;
    SDIO->DCTRL = 0x00000000;
    SDIO->ICF = 0x000000000;
    SDIO->MASK = 0x00000000;
    SDIO->FIFODATA = 0x00000000;
}

/*!
 * @brief       SD Card Init
 *
 * @param       sdInfo
 *
 * @retval      SD status
 *
 */
SD_STATUS_T SD_Init(SD_CARD_INFO_T *sdInfo)
{
    SD_STATUS_T status = SD_OK;
    
    status = SD_InitCard(sdInfo);
    if(status != SD_OK)
    {
        return SD_ERR;
    }
    
    /* Set SDIO bus wide as 4B */
    SD_EnableWideBusOperation(sdInfo, SDIO_BUS_WIDE_4B);
    
    return status;
}

/*!
 * @brief       Show SD card informations.
 *
 * @param       sdInfo
 *
 * @retval      None
 *
 */
void SD_ShowCardInfo(SD_CARD_INFO_T *sdInfo)
{
    switch(sdInfo->cardType)
    {
        case SD_STD_CAPACITY_SD_CARD:
            printf("Card Type:SDSC V1.1\r\n");
        break;

        case SD_HIGH_CAPACITY_SD_CARD:
            printf("Card Type:SDHC V2.0\r\n");
        break;
    }
    
    /* Manufacturer ID */
    printf("Card ManufacturerID:%d\r\n",(int)(sdInfo->cid[0] & 0xFF000000) >> 24);

    /* Ralative card address */
    printf("Card RCA:%d\r\n",(int)sdInfo->rca);

    /* Card block size */
    printf("Card BlockSize:%d\r\n",(int)sdInfo->blockSize);
    
    /* Card block number */
    printf("Card BlockNum:%d\r\n\r\n",(int)sdInfo->blockNum);
}

/*!
 * @brief       SD Card Read blocks
 *
 * @param       sdInfo
 *
 * @param       data: save the read data
 *
 * @param       blkAddr: read block address
 *
 * @param       blkNum: block number
 *
 * @param       timeout: timeout
 *
 * @retval      SD status
 *
 */
SD_STATUS_T SD_ReadBlocks(SD_CARD_INFO_T *sdInfo, uint8_t *data, uint32_t blkAddr, uint32_t blkNum, uint32_t timeout)
{
    SD_STATUS_T status = SD_OK;
    SD_ERR_T errStatus = SD_ERR_NONE;
    
    SDIO_DataConfig_T sdioDataConfigStructure;
    uint32_t remain;
    uint32_t dataTemp;
    uint32_t i;
    uint32_t blkAddrTemp = blkAddr;
    uint8_t *pdata = data;
    
    uint32_t tick = APM_ReadTick();
    
    if(data == NULL)
    {
        return SD_ERR;
    }
    
    if(sdInfo->state == SD_STATE_READY)
    {
        if((blkAddrTemp + blkNum) > (sdInfo->logBlockNum))
        {
            errStatus = SD_ADDR_OUT_OF_RANGE;
            return SD_ERR;
        }
        
        sdInfo->state = SD_STATE_BUSY;
        
        /* Clear data control register */
        SDIO->DCTRL = 0;
        
        if(sdInfo->cardType != SD_HIGH_CAPACITY_SD_CARD)
        {
            blkAddrTemp *= SD_BLOCK_SIZE;
        }
        
        /* Configure DPSM */
        sdioDataConfigStructure.dataBlockSize   = SDIO_DATA_BLOCKSIZE_512B;
        sdioDataConfigStructure.dataLength      = blkNum * SD_BLOCK_SIZE;
        sdioDataConfigStructure.dataTimeOut     = SD_DATATIMEOUT;
        sdioDataConfigStructure.DPSM            = SDIO_DPSM_ENABLE;
        sdioDataConfigStructure.transferDir     = SDIO_TRANSFER_DIR_TO_SDIO;
        sdioDataConfigStructure.transferMode    = SDIO_TRANSFER_MODE_BLOCK;
        SDIO_ConfigData(&sdioDataConfigStructure);
        
        if(blkNum > 1)
        {
            /* Send Read Multi Block command */
            errStatus = SD_SendCmdReadMultiBlock(blkAddrTemp);
        }
        else
        {
            /* Send Read Single Block command */
            errStatus = SD_SendCmdReadSingleBlock(blkAddrTemp);
        }
        
        if(errStatus != SD_ERR_NONE)
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        
        /* Poll */
        remain = sdioDataConfigStructure.dataLength;
        
        while(!(SDIO->STS & (SDIO_FLAG_RXOVRER | \
                                     SDIO_FLAG_DBDR    | \
                                     SDIO_FLAG_DATATO  | \
                                     SDIO_FLAG_DATAEND | \
                                     SDIO_FLAG_SBE)))
        {
            if((SDIO_ReadStatusFlag(SDIO_FLAG_RXFHF) != RESET) && (remain > 0))
            {
                for(i = 0; i < 8; i++)
                {
                    dataTemp = SDIO_ReadData();
                    *pdata = (uint8_t)(dataTemp & 0xFF);
                    pdata++;
                    remain--;
                    
                    *pdata = (uint8_t)((dataTemp >> 8) & 0xFF);
                    pdata++;
                    remain--;
                    
                    *pdata = (uint8_t)((dataTemp >> 16) & 0xFF);
                    pdata++;
                    remain--;
                    
                    *pdata = (uint8_t)((dataTemp >> 24) & 0xFF);
                    pdata++;
                    remain--;
                }
            }
            
            if(((APM_ReadTick() - tick) >= timeout) || (timeout == 0))
            {
                SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
                sdInfo->state = SD_STATE_READY;
                return SD_TIMEOUT;
            }
        }
        
        /* Send Stop transmission command */
        if(SDIO_ReadStatusFlag(SDIO_FLAG_DATAEND) && (blkNum > 1))
        {
            if(sdInfo->cardType != SD_SECURE_DIGITAL_IO_CARD)
            {
                errStatus = SD_SendCmdStopTransfer();
                if(errStatus != SD_ERR_NONE)
                {
                    SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
                    sdInfo->state = SD_STATE_READY;
                    return SD_ERR;
                }
            }
        }

        /* Handle Error State */
        if(SDIO_ReadStatusFlag(SDIO_FLAG_DATATO))
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            errStatus = SD_ERR_CMD_RSP_TIMEOUT;
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        else if(SDIO_ReadStatusFlag(SDIO_FLAG_DBDR))
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            errStatus = SD_DATA_CRC_FAIL;
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        else if(SDIO_ReadStatusFlag(SDIO_FLAG_RXOVRER))
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            errStatus = SD_RX_OVERRUN;
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        else
        {
            
        }

        /* Clear FIFO data */
        while(SDIO_ReadStatusFlag(SDIO_FLAG_RXDA) && (remain > 0))
        {
            dataTemp = SDIO_ReadData();
            *pdata = (uint8_t)(dataTemp & 0xFF);
            pdata++;
            remain--;
            
            *pdata = (uint8_t)((dataTemp >> 8) & 0xFF);
            pdata++;
            remain--;
            
            *pdata = (uint8_t)((dataTemp >> 16) & 0xFF);
            pdata++;
            remain--;
            
            *pdata = (uint8_t)((dataTemp >> 24) & 0xFF);
            pdata++;
            remain--;
            
            if(((APM_ReadTick() - tick) >= timeout) || (timeout == 0))
            {
                SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
                sdInfo->state = SD_STATE_READY;
                return SD_TIMEOUT;
            }
        }
        
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        
        sdInfo->state = SD_STATE_READY;
        
        status = SD_OK;
    }
    else
    {
        return SD_ERR;
    }
    
    return status;
}

/*!
 * @brief       SD Card Write blocks
 *
 * @param       sdInfo
 *
 * @param       data: save the read data
 *
 * @param       blkAddr: read block address
 *
 * @param       blkNum: block number
 *
 * @param       timeout: timeout
 *
 * @retval      SD status
 *
 */
SD_STATUS_T SD_WriteBlocks(SD_CARD_INFO_T *sdInfo, uint8_t *data, uint32_t blkAddr, uint32_t blkNum, uint32_t timeout)
{
    SD_STATUS_T status = SD_OK;
    SD_ERR_T errStatus = SD_ERR_NONE;
    
    SDIO_DataConfig_T sdioDataConfigStructure;
    uint32_t remain;
    uint32_t dataTemp;
    uint8_t i;
    uint32_t blkAddrTemp = blkAddr;
    __IO uint8_t *pdata = data;
    
    __IO uint32_t tick = APM_ReadTick();
    
    if(data == NULL)
    {
        return SD_ERR;
    }
    
    if(sdInfo->state == SD_STATE_READY)
    {
        if((blkAddrTemp + blkNum) > (sdInfo->logBlockNum))
        {
            errStatus = SD_ADDR_OUT_OF_RANGE;
            return SD_ERR;
        }
        
        sdInfo->state = SD_STATE_BUSY;
        
        /* Clear data control register */
        SDIO->DCTRL = 0;
        
        if(sdInfo->cardType != SD_HIGH_CAPACITY_SD_CARD)
        {
            blkAddrTemp *= SD_BLOCK_SIZE;
        }
        
        /* Configure DPSM */
        sdioDataConfigStructure.dataBlockSize   = SDIO_DATA_BLOCKSIZE_512B;
        sdioDataConfigStructure.dataLength      = blkNum * SD_BLOCK_SIZE;
        sdioDataConfigStructure.dataTimeOut     = SD_DATATIMEOUT;
        sdioDataConfigStructure.DPSM            = SDIO_DPSM_ENABLE;
        sdioDataConfigStructure.transferDir     = SDIO_TRANSFER_DIR_TO_CARD;
        sdioDataConfigStructure.transferMode    = SDIO_TRANSFER_MODE_BLOCK;
        SDIO_ConfigData(&sdioDataConfigStructure);
        
        if(blkNum > 1)
        {
            /* Send Write Multi Block command */
            errStatus = SD_SendCmdWriteMultiBlock(blkAddrTemp);
        }
        else
        {
            /* Send Write Single Block command */
            errStatus = SD_SendCmdWriteSingleBlock(blkAddrTemp);
        }
        
        if(errStatus != SD_ERR_NONE)
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        
        /* Poll */
        remain = sdioDataConfigStructure.dataLength;
        
        while(!(SDIO->STS & (SDIO_FLAG_TXUDRER | \
                                     SDIO_FLAG_DBDR    | \
                                     SDIO_FLAG_DATATO  | \
                                     SDIO_FLAG_DATAEND | \
                                     SDIO_FLAG_SBE)))
        {
            if((SDIO_ReadStatusFlag(SDIO_FLAG_TXFHF) != RESET) && remain > 0)
            {
                for(i = 0; i < 8; i++)
                {
                    dataTemp = (uint32_t)(*pdata);
                    pdata++;
                    remain--;
                    
                    dataTemp |= ((uint32_t)(*pdata) << 8);
                    pdata++;
                    remain--;
                    
                    dataTemp |= ((uint32_t)(*pdata) << 16);
                    pdata++;
                    remain--;
                    
                    dataTemp |= ((uint32_t)(*pdata) << 24);
                    pdata++;
                    remain--;
                    
                    SDIO_WriteData(dataTemp);
                }
            }
            
            if(((APM_ReadTick() - tick) >= timeout) || (timeout == 0))
            {
                SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
                sdInfo->state = SD_STATE_READY;
                return SD_TIMEOUT;
            }
        }
        
        /* Send Stop transmission command */
        if(SDIO_ReadStatusFlag(SDIO_FLAG_DATAEND) && (blkNum > 1))
        {
            if(sdInfo->cardType != SD_SECURE_DIGITAL_IO_CARD)
            {
                errStatus = SD_SendCmdStopTransfer();
                if(errStatus != SD_ERR_NONE)
                {
                    SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
                    sdInfo->state = SD_STATE_READY;
                    return SD_ERR;
                }
            }
        }
        
        /* Handle Error State */
        if(SDIO_ReadStatusFlag(SDIO_FLAG_DATATO))
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            errStatus = SD_ERR_CMD_RSP_TIMEOUT;
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        else if(SDIO_ReadStatusFlag(SDIO_FLAG_DBDR))
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            errStatus = SD_DATA_CRC_FAIL;
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        else if(SDIO_ReadStatusFlag(SDIO_FLAG_TXUDRER))
        {
            SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
            errStatus = SD_TX_UNDERRUN;
            sdInfo->state = SD_STATE_READY;
            return SD_ERR;
        }
        else
        {
            
        }
        
        SDIO_ClearStatusFlag(SDIO_STATIC_FLAGS);
        
        sdInfo->state = SD_STATE_READY;
    }
    else
    {
        return SD_ERR;
    }
    
    return status;
}

/*!
 * @brief       SD Card Read Card State
 *
 * @param       sdInfo
 *
 * @retval      SD state
 *
 */
uint32_t SD_ReadCardState(SD_CARD_INFO_T *sdInfo)
{
    SD_ERR_T errStatus = SD_ERR_NONE;
    
    uint32_t response = 0;
    
    errStatus = SD_SendCmdSendStatus((uint32_t)sdInfo->rca << 16);
    if(errStatus != SD_ERR_NONE)
    {
        
    }
    
    response = SDIO_ReadResponse(SDIO_RES1);
    
    response = ((response >> 9) & 0x0F);
    
    return response;
}

/*!
 * @brief       SD Read State
 *
 * @param       sdInfo
 *
 * @retval      SD state
 *
 */
uint32_t SD_ReadState(SD_CARD_INFO_T *sdInfo)
{
    return sdInfo->state;
}

/**@} end of group APM32F407_EVAL_Functions */
/**@} end of group Board_APM32F407_EVAL */
/**@} end of group Board */
