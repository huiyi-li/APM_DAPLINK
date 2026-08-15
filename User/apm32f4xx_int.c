/*!
 * @file        apm32f4xx_int.c
 *
 * @brief       Main Interrupt Service Routines
 *
 * @version     V1.0.3
 *
 * @date        2023-07-31
 *
 * @attention
 *
 *  Copyright (C) 2021-2023 Geehy Semiconductor
 *
 *  You may not use this file except in compliance with the
 *  GEEHY COPYRIGHT NOTICE (GEEHY SOFTWARE PACKAGE LICENSE).
 *
 *  The program is only for reference, which is distributed in the hope
 *  that it will be useful and instructional for customers to develop
 *  their software. Unless required by applicable law or agreed to in
 *  writing, the program is distributed on an "AS IS" BASIS, WITHOUT
 *  ANY WARRANTY OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the GEEHY SOFTWARE PACKAGE LICENSE for the governing permissions
 *  and limitations under the License.
 */

#include "main.h"
#include "apm32f4xx_int.h"
#include "core_cm4.h"

/** @addtogroup Examples
  @{
  */

/** @addtogroup Template
  @{
  */

/** @defgroup Template_INT_Functions INT_Functions
  @{
  */

/*!
 * @brief   This function handles NMI exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
void NMI_Handler(void)
{
}

/*!
 * @brief   This function handles Hard Fault exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
static void hf_hex32(uint32_t val)
{
    static const char hexdig[] = "0123456789ABCDEF";
    uint8_t buf[11] = { '0', 'x', 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 9; i >= 2; i--)
    {
        buf[i] = (uint8_t)hexdig[val & 0xF];
        val >>= 4;
    }
    extern void bsp_debug_uart_write(const uint8_t *data, uint32_t len);
    bsp_debug_uart_write(buf, 10);
}

void HardFault_Handler(void)
{
    uint32_t hfsr = SCB->HFSR;
    uint32_t cfsr = SCB->CFSR;
    uint32_t bfar = SCB->BFAR;
    volatile uint32_t *stack = (volatile uint32_t *)__get_MSP();
    extern void bsp_debug_uart_write(const uint8_t *data, uint32_t len);
    static const uint8_t h1[] = "\r\n[HF] HFSR=";
    static const uint8_t h2[] = " CFSR=";
    static const uint8_t h3[] = " BFAR=";
    static const uint8_t h4[] = " PSR=";
    static const uint8_t h5[] = " PC=";
    static const uint8_t h6[] = " LR=";
    static const uint8_t h7[] = " MSP=";

    bsp_debug_uart_write(h1, sizeof(h1) - 1); hf_hex32(hfsr);
    bsp_debug_uart_write(h2, sizeof(h2) - 1); hf_hex32(cfsr);
    bsp_debug_uart_write(h3, sizeof(h3) - 1); hf_hex32(bfar);
    bsp_debug_uart_write(h4, sizeof(h4) - 1); hf_hex32(stack[7]);
    bsp_debug_uart_write(h5, sizeof(h5) - 1); hf_hex32(stack[6]);
    bsp_debug_uart_write(h6, sizeof(h6) - 1); hf_hex32(stack[5]);
    bsp_debug_uart_write(h7, sizeof(h7) - 1); hf_hex32((uint32_t)stack);

    while (1)
    {
    }
}

/*!
 * @brief   This function handles Memory Manage exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1)
    {
    }
}

/*!
 * @brief   This function handles Bus Fault exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
void BusFault_Handler(void)
{
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1)
    {
    }
}

/*!
 * @brief   This function handles Usage Fault exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
void UsageFault_Handler(void)
{
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1)
    {
    }
}

/*!
 * @brief   This function handles SVCall exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
// void SVC_Handler(void)
// {
// }

/*!
 * @brief   This function handles Debug Monitor exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
void DebugMon_Handler(void)
{
}

/*!
 * @brief   This function handles PendSV_Handler exception
 *
 * @param   None
 *
 * @retval  None
 *
 */
// void PendSV_Handler(void)
// {
// }

/*!
 * @brief   This function handles SysTick Handler
 *
 * @param   None
 *
 * @retval  None
 *
 */
// void SysTick_Handler(void)
// {
// }

/**@} end of group Template_INT_Functions */
/**@} end of group Template */
/**@} end of group Examples */

