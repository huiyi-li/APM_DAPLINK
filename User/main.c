/*!
 * @file        main.c
 *
 * @brief       Main program body
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

/* Includes */
#include "main.h"
#include <string.h>
#include <stdio.h>
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "bsp_sysclk.h"
#include <core_cm4.h>
#include "bsp_printf.h"
#include "tx_api.h"
#include "user_usb_init.h"
#include "usbd_core.h"
#include "dap_main.h"
#include "bsp_led.h"
#include "bsp_button.h"
#include "bsp_cdc_uart.h"
#include "bsp_uart1.h"
#include "bsp_w25qxx.h"
#include "display_port.h"

#define VECT_TAB_OFFSET  0x00

// void __attribute__((constructor)) FPU_Init(void) 
// {
//     #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
//     SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  //!< set CP10 and CP11 Full Access
//     #endif
//     /* Configure the Vector Table location add offset address */
//     #ifdef VECT_TAB_SRAM
//     SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM */
//     #else
//     SCB->VTOR = FMC_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH */
//     #endif
// }

TX_THREAD               thread_0;
TX_THREAD               ThreadInit;
TX_THREAD               thread_2;
TX_THREAD               thread_3;
TX_THREAD               thread_4;
TX_THREAD               thread_5;
TX_THREAD               thread_6;
TX_THREAD               thread_7;
TX_QUEUE                queue_0;
TX_SEMAPHORE            semaphore_0;
TX_MUTEX                mutex_0;
TX_EVENT_FLAGS_GROUP    event_flags_0;
TX_BYTE_POOL            byte_pool_0;
TX_BLOCK_POOL           block_pool_0;

#define DEMO_STACK_SIZE         1024
#define DEMO_BYTE_POOL_SIZE     10240
#define DEMO_BLOCK_POOL_SIZE    100
#define DEMO_QUEUE_SIZE         100

void    thread_0_entry(ULONG thread_input)
{
    (void)thread_input;
    while(1)
    {
        const ULONG now = tx_time_get();

        bsp_button_process((uint32_t)now);
        tx_thread_sleep(10U);
    }
}

void InitThread(ULONG thread_input)
{
    DISPLAY_PORT_STATUS_T display_status;

    (void)thread_input;

    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOB);
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOC);
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOE);
    // chry_dap_init(0,USB_OTG_FS_BASE);
    chry_dap_init(0,USB_OTG_HS_BASE);
    display_status = display_port_init();
    printf("ST7789 init: %s\r\n",
           (display_status == DISPLAY_PORT_OK) ? "ok" : "failed");
//    void cdc_acm_init(uint8_t busid, uintptr_t reg_base);
//    cdc_acm_init(0,USB_OTG_FS_BASE);
//    cdc_acm_init(0,USB_OTG_HS_BASE);
     while(1){
//        cdc_acm_data_send_with_dtr_test();
    chry_dap_handle();
    chry_dap_usb2uart_handle();

    tx_thread_sleep(1U);
     }
}

void tx_application_define(void *first_unused_memory)
{
    CHAR    *pointer = TX_NULL;
    CHAR    *InitTaskPtr = TX_NULL;


    /* Create a byte memory pool from which to allocate the thread stacks.  */
    tx_byte_pool_create(&byte_pool_0, "byte pool 0", first_unused_memory, DEMO_BYTE_POOL_SIZE);

    /* Put system definition stuff in here, e.g. thread creates and other assorted
       create information.  */

    /* Allocate the stack for thread 0.  */
    tx_byte_allocate(&byte_pool_0, (VOID **) &pointer, DEMO_STACK_SIZE, TX_NO_WAIT);
    tx_byte_allocate(&byte_pool_0, (VOID **) &InitTaskPtr, 2048, TX_NO_WAIT);

    /* Create the main thread.  */
    tx_thread_create(&thread_0, "thread 0", thread_0_entry, 0,
            pointer, DEMO_STACK_SIZE,
            0, 0, TX_NO_TIME_SLICE, TX_AUTO_START);

     tx_thread_create(&ThreadInit, "thread Init", InitThread, 0,
            InitTaskPtr, 2048,
            6, 4, TX_NO_TIME_SLICE, TX_AUTO_START);

}

VOID _tx_thread_stack_error_handler(TX_THREAD *thread_ptr)
{
    (void)thread_ptr;
}

const char * const g_hello = "Hello, string is Initialized!\r\n";
/*!
 * @brief       Main program
 *
 * @param       None
 *
 * @retval      None
 */
int main(void)
{
    BSP_W25Q64_STATUS_T flash_status;
    uint32_t flash_jedec_id = 0U;

    bsp_sysclk_init();
    bsp_led_init();
    bsp_button_init(NULL);
    bsp_debug_uart_init(115200U);
    (void)bsp_cdc_uart_init(NULL);
    bsp_uart1_init(115200U);
    bsp_debug_uart_write((const uint8_t *)g_hello, strlen(g_hello));
    printf("Hello, world!\r\n");
    flash_status = bsp_w25q64_init();
    if (flash_status == BSP_W25Q64_OK)
    {
        (void)bsp_w25q64_read_jedec_id(&flash_jedec_id);
        printf("W25Q64 JEDEC ID: %06lX\r\n", (unsigned long)flash_jedec_id);
    }
    else
    {
        printf("W25Q64 init failed: %d\r\n", (int)flash_status);
    }
    tx_kernel_enter();

    while (1)
    {
    }
}

/**@} end of group Template_Functions */
/**@} end of group Template */
/**@} end of group Examples */
