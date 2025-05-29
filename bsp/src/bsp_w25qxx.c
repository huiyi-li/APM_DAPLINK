/*!
 * @file        bsp_w25qxx.c
 *
 * @brief       W25QXX board support package program body
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

/* Includes */
#include "bsp_w25qxx.h"
#include "stdio.h"
#include "math.h"

W25QXX_INFO_T w25qxxInfo;
SPI_Config_T  SPIx_InitStructure;

static void W25QXX_SPI_Init(void);
static uint8_t W25QXX_SPI_ReadWriteByte(uint8_t data);
static uint8_t W25QXX_ReadStatusReg(uint8_t regNum);
static void W25QXX_EnableWrite(void);
static void W25QXX_WritePage(uint8_t* buffer,uint32_t addr,uint16_t length);
static void W25QXX_WritePageFast(uint8_t* buffer,uint32_t addr,uint16_t length);
static void W25QXX_WaitBusy(void);

/**
 * @brief   W25QXX SPI Write and Read Byte
 * @param   data
 * @retval  receive data
 */
static uint8_t W25QXX_SPI_ReadWriteByte(uint8_t data)
{
    uint8_t rxData;
    
    while( ((W25QXX_SPI->STS) & (SPI_FLAG_TXBE)) == RESET);
        
    W25QXX_SPI->DATA = data;
    
    while( ((W25QXX_SPI->STS) & (SPI_FLAG_RXBNE)) == RESET);
    
    rxData = W25QXX_SPI->DATA;
    
    return rxData;
}

/**
 * @brief   W25QXX SPI Write and Read Byte
 * @param   data
 * @retval  receive data
 */
static void W25QXX_SPI_Init(void)
{
    GPIO_Config_T GPIO_InitStructure;

    /* Enable Clock */
    W25QXX_SPI_GPIO_CLK_ENABLE();
    W25QXX_SPI_CLK_ENABLE();
    RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SYSCFG);
    
    /* Config SPI PinAF */
    GPIO_ConfigPinAF(W25QXX_SPI_GPIO_PORT, GPIO_PIN_SOURCE_3, GPIO_AF_SPI1);
    GPIO_ConfigPinAF(W25QXX_SPI_GPIO_PORT, GPIO_PIN_SOURCE_4, GPIO_AF_SPI1);
    GPIO_ConfigPinAF(W25QXX_SPI_GPIO_PORT, GPIO_PIN_SOURCE_5, GPIO_AF_SPI1);
    
    GPIO_InitStructure.pin              = W25QXX_SPI_SCK_GPIO_PIN | W25QXX_SPI_MISO_GPIO_PIN | W25QXX_SPI_MOSI_GPIO_PIN;
    GPIO_InitStructure.mode             = GPIO_MODE_AF;
    GPIO_InitStructure.otype            = GPIO_OTYPE_PP;
    GPIO_InitStructure.pupd             = GPIO_PUPD_UP;
    GPIO_InitStructure.speed            = GPIO_SPEED_50MHz;
    GPIO_Config(W25QXX_SPI_GPIO_PORT,&GPIO_InitStructure);
    
    SPIx_InitStructure.mode             = SPI_MODE_MASTER;
    SPIx_InitStructure.direction        = SPI_DIRECTION_2LINES_FULLDUPLEX;
    SPIx_InitStructure.length           = SPI_DATA_LENGTH_8B;
    SPIx_InitStructure.polarity         = SPI_CLKPOL_HIGH;
    SPIx_InitStructure.phase            = SPI_CLKPHA_2EDGE;
    SPIx_InitStructure.nss              = SPI_NSS_SOFT;
    SPIx_InitStructure.baudrateDiv      = SPI_BAUDRATE_DIV_4;
    SPIx_InitStructure.firstBit         = SPI_FIRSTBIT_MSB;
    SPIx_InitStructure.crcPolynomial    = 7;
    SPI_Config(W25QXX_SPI, &SPIx_InitStructure);

    SPI_Enable(W25QXX_SPI);

    W25QXX_SPI_ReadWriteByte(0xFFU);
}

/**
 * @brief   W25QXX Read Status Register
 * @param   regNum
 * @retval  status data
 */
static uint8_t W25QXX_ReadStatusReg(uint8_t regNum)
{
    uint8_t status = 0U;
    uint8_t cmd = 0U;
    
    switch(regNum)
    {
        case W25QXX_STATUS_REG_1:
            cmd = W25QXX_CMD_READ_STA_REG1;
            break;
        
        case W25QXX_STATUS_REG_2:
            cmd = W25QXX_CMD_READ_STA_REG2;
            break;
        
        case W25QXX_STATUS_REG_3:
            cmd = W25QXX_CMD_READ_STA_REG3;
            break;
        
        default:
            cmd = W25QXX_CMD_READ_STA_REG1;
            break;
    }

    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(cmd);
    status = W25QXX_SPI_ReadWriteByte(0xFFU);
    W25QXX_CS_Disable();
    
    return status;
} 

/**
 * @brief   W25QXX Enable Write
 * @param   None
 * @retval  None
 */
static void W25QXX_EnableWrite(void)
{
    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_ENABLE_WRITE);
    W25QXX_CS_Disable();
}

/**
 * @brief   W25QXX Wait Busy
 * @param   None
 * @retval  None
 */
static void W25QXX_WaitBusy(void)
{
    while((W25QXX_ReadStatusReg(W25QXX_STATUS_REG_1) & 0x01U) == 0x01U)
    {
        
    }
}

/**
 * @brief   Read W25QXX Flash ID
 * @param   None
 * @retval  flash ID
 */
uint16_t W25QXX_ReadFlashID(void)
{
    uint16_t id = 0U;
    
    W25QXX_CS_Enable();
    
    W25QXX_SPI_ReadWriteByte(W25QXX_MFC_DEV_ID);
    W25QXX_SPI_ReadWriteByte(0x00U);
    W25QXX_SPI_ReadWriteByte(0x00U);
    W25QXX_SPI_ReadWriteByte(0x00U);
    id |= W25QXX_SPI_ReadWriteByte(0xFFU) << 8U;
    id |= W25QXX_SPI_ReadWriteByte(0xFFU);
    
    W25QXX_CS_Disable();
    
    return id;
}

/**
 * @brief   W25QXX Read Data
 * @param   buffer
 * @param   addr Specify address to reading data
 * @param   length Specify length to reading data
 * @retval  None
 */
void W25QXX_ReadData(uint8_t* buffer,uint32_t addr,uint16_t length)
{ 
    uint16_t i;
    
    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_READ_DATA);
    
    /* Send High Address */
    if(w25qxxInfo.flashID == W25Q256_FLASH_ID)
    {
        W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 24U));
    }
    
    W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 16U));
    W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 8U));
    W25QXX_SPI_ReadWriteByte((uint8_t)addr);
    
    for(i = 0U; i < length; i++)
    { 
        buffer[i] = W25QXX_SPI_ReadWriteByte(0xFFU);
    }
    
    W25QXX_CS_Disable();
}

/**
 * @brief   W25QXX write page data
 * @param   buffer
 * @param   addr Specify address to write data
 * @param   length Specify length to write data
 * @retval  None
 */
static void W25QXX_WritePage(uint8_t* buffer,uint32_t addr,uint16_t length)
{
    uint16_t i;
    
    W25QXX_EnableWrite();
    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_PAGE_PROGRAM);
    
    /* Send High Address */
    if(w25qxxInfo.flashID == W25Q256_FLASH_ID)
    {
        W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 24U));
    }
    
    W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 16U));
    W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 8U));
    W25QXX_SPI_ReadWriteByte((uint8_t)addr);
    
    for(i = 0U;i < length; i++)
    {
        W25QXX_SPI_ReadWriteByte(buffer[i]);
    }
    
    W25QXX_CS_Disable();
    W25QXX_WaitBusy();
}

/**
 * @brief   W25QXX Fast Write Page
 * @param   buffer
 * @param   addr Specify address to write data
 * @param   length Specify length to write data
 * @retval  None
 * @note    Make sure that all the data in the written address range is 0XFF, otherwise the data written at non-0XFF will fail
 */
static void W25QXX_WritePageFast(uint8_t* buffer,uint32_t addr,uint16_t length)
{
    uint16_t remain = 0U;
    
    /* Number of bytes remain on a page */
    remain = 256U - addr % 256U;
    
    remain = length <= remain ? length : remain;
    
    while(1)
    {
        W25QXX_WritePage(buffer,addr,remain);
        
        if(length == remain)
        {
            break;
        }
        else
        {
            buffer += remain;
            addr += remain;
            length -= remain;
            
            remain = length > 256U ? 256U : length;
        }
    }
}

/**
 * @brief   W25QXX Write Data
 * @param   buffer
 * @param   addr Specify address to write data
 * @param   length Specify length to write data
 * @retval  None
 */
void W25QXX_WriteData(uint8_t* buffer,uint32_t addr,uint16_t length)
{
    uint32_t sectorAddr = addr / W25QXX_SECTOR_SIZE;
    uint16_t sectorOffset = addr % W25QXX_SECTOR_SIZE;
    uint16_t sectorRemain = W25QXX_SECTOR_SIZE - sectorOffset;
    uint16_t i;
    uint8_t *writeBuffer;
    
    writeBuffer = w25qxxInfo.writeBuffer;
    
    sectorRemain = length <= sectorRemain ? length : sectorRemain;
    
    while(1)
    {
        /* Read all data of sector */
        W25QXX_ReadData(writeBuffer,sectorAddr * W25QXX_SECTOR_SIZE, W25QXX_SECTOR_SIZE);
        
        /* Check Data */
        for(i = 0U; i < sectorRemain; i++)
        {
            if(writeBuffer[sectorOffset + i] != 0xFFU)
            {
                break;
            }
        }
        
        if(i < sectorRemain)
        {
            W25QXX_EraseSector(sectorAddr);

            for(i = 0U; i < sectorRemain; i++)
            {
                writeBuffer[i + sectorOffset] = buffer[i];
            }
            
            W25QXX_WritePageFast(writeBuffer,sectorAddr * W25QXX_SECTOR_SIZE, W25QXX_SECTOR_SIZE);

        }
        else
        {
            W25QXX_WritePageFast(buffer,addr,sectorRemain);
        }
        
        if(length == sectorRemain)
        {
            break;
        }
        else
        {
            sectorAddr++;
            sectorOffset = 0U;

            buffer += sectorRemain;
            addr += sectorRemain;
            length -= sectorRemain;
            
            sectorRemain = length > W25QXX_SECTOR_SIZE ? W25QXX_SECTOR_SIZE : length;
        }
    }
}

/**
 * @brief   W25QXX Erase Whole Chip
 * @param   None
 * @retval  None
 */
void W25QXX_EraseChip(void)
{
    W25QXX_EnableWrite();
    W25QXX_WaitBusy();
    W25QXX_CS_Enable();
    
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_CHIP_ERASE);
    
    W25QXX_CS_Disable();
    W25QXX_WaitBusy();
}

/**
 * @brief   W25QXX Erase Sector
 * @param   addr Specify address to erase
 * @retval  None
 */
void W25QXX_EraseSector(uint32_t addr)
{
    addr *= W25QXX_SECTOR_SIZE;
    
    W25QXX_EnableWrite();
    W25QXX_WaitBusy();
    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_SECTOR_ERASE);
    
    if(w25qxxInfo.flashID == W25Q256_FLASH_ID)
    {
        W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 24U));
    }
    
    W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 16U));
    W25QXX_SPI_ReadWriteByte((uint8_t)((addr) >> 8U));
    W25QXX_SPI_ReadWriteByte((uint8_t)addr);
    W25QXX_CS_Disable();
    W25QXX_WaitBusy();
}

/**
 * @brief   W25QXX Power Down
 * @param   addr Specify address to erase
 * @retval  None
 */
void W25QXX_PowerDown(void)
{
    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_PWR_DOWN);
    W25QXX_CS_Disable();
    W25QXX_Delay(1);
}

/**
 * @brief   W25QXX Wakeup
 * @param   addr Specify address to erase
 * @retval  None
 */
void W25QXX_Wakeup(void)
{
    W25QXX_CS_Enable();
    W25QXX_SPI_ReadWriteByte(W25QXX_CMD_REL_PWR_DOWN);
    W25QXX_CS_Disable();
    W25QXX_Delay(1);
}

/**
 * @brief   Init W25QXX
 * @param   type Flash type
 *              @arg W25Q80_FLASH
 *              @arg W25Q16_FLASH
 *              @arg W25Q32_FLASH
 *              @arg W25Q64_FLASH
 *              @arg W25Q128_FLASH
 *              @arg W25Q256_FLASH
 * @retval  None
 * @note    W25Q256 size = 32MB = 512 blocks = 8192 sectors
 *          1 sector = 4096 bytes
 *          1 block = 16 sector
 */
void W25QXX_Init(W25QXX_TYPE_T type)
{
    uint8_t addrMode = 0U;
    
    GPIO_Config_T GPIO_InitStructure;
    
    /* Enable W25QXX CS Clock */
    W25QXX_CS_GPIO_CLK_ENABLE();
    
    GPIO_InitStructure.pin    = W25QXX_CS_GPIO_PIN;
    GPIO_InitStructure.mode   = GPIO_MODE_OUT;
    GPIO_InitStructure.otype  = GPIO_OTYPE_PP;
    GPIO_InitStructure.pupd   = GPIO_PUPD_UP;
    GPIO_InitStructure.speed  = GPIO_SPEED_50MHz;
    GPIO_Config(W25QXX_CS_GPIO_PORT,&GPIO_InitStructure);
    
    W25QXX_CS_Disable();
    W25QXX_SPI_Init();
    
    w25qxxInfo.type = type;
    w25qxxInfo.flashID = W25QXX_ReadFlashID();
    
    w25qxxInfo.flashSize = (uint32_t)powf(2U, (w25qxxInfo.flashID - W25Q80_FLASH_ID)) * 1024U * 1024U;
    w25qxxInfo.sectorSize = W25QXX_SECTOR_SIZE;
    w25qxxInfo.sectorNum = w25qxxInfo.flashSize / w25qxxInfo.sectorSize;
    
    printf("flash id = %04X, flash size = %d, sector num = %d\r\n",w25qxxInfo.flashID, (int)w25qxxInfo.flashSize, (int)w25qxxInfo.sectorNum);
    
    if(w25qxxInfo.flashID == W25Q128_FLASH_ID) 
    {
        /* Status Register 3 */
        addrMode = W25QXX_ReadStatusReg(W25QXX_STATUS_REG_3);
        /* Address mode */
        if((addrMode & 0x01U) == 0U)
        {
            W25QXX_CS_Enable();
            W25QXX_SPI_ReadWriteByte(W25QXX_ENABLE_4_BYTE_ADDR);
            W25QXX_CS_Disable();
        }
    }
}
