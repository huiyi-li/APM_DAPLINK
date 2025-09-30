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
#include <stdlib.h>
#include "apm32f4xx_gpio.h"
#include "apm32f4xx_rcm.h"
#include "bsp_sysclk.h"
#include <core_cm4.h>
#include "bsp_printf.h"
#include "tx_api.h"
#include "user_usb_init.h"
#include "usbd_core.h"
#include "dap_main.h"

#include "bsp_uart1.h"
/** @addtogroup Examples
  @{
  */

/** @addtogroup Template
  @{
  */

/** @defgroup Template_Functions Functions
  @{
  */
#define RCM_LED   RCM_AHB1_PERIPH_GPIOC
#define LED_PORT  GPIOC
#define LED_PIN   GPIO_PIN_0
#define VECT_TAB_OFFSET  0x00

void GpioLedInit(void)
{
    GPIO_Config_T  configStruct;

    /* Enable the GPIO_LED Clock */
    RCM_EnableAHB1PeriphClock(RCM_LED);

    /* Configure the GPIO_LED pin */
    GPIO_ConfigStructInit(&configStruct);
    configStruct.pin = LED_PIN;
    configStruct.mode = GPIO_MODE_OUT;
    configStruct.speed = GPIO_SPEED_50MHz;

    GPIO_Config(LED_PORT, &configStruct);
    GPIO_ResetBit(LED_PORT, LED_PIN);
}

void __attribute__((constructor)) FPU_Init(void) 
{
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  //!< set CP10 and CP11 Full Access
    #endif
    /* Configure the Vector Table location add offset address */
    #ifdef VECT_TAB_SRAM
    SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM */
    #else
    SCB->VTOR = FMC_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH */
    #endif
}

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

    UINT    status;
    /* This thread simply sits in while-forever-sleep loop.  */
    while(1)
    {

        /* Increment the thread counter.  */
//        thread_0_counter++;
        GPIO_ToggleBit(LED_PORT, LED_PIN);
        /* Sleep for 10 ticks.  */
        tx_thread_sleep(1000);
    }
}

void InitThread(ULONG thread_input)
{

    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOB);
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOC);
    RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOE);
//    chry_dap_init(0,USB_OTG_FS_BASE);
    chry_dap_init(0,USB_OTG_HS_BASE);
//    void cdc_acm_init(uint8_t busid, uintptr_t reg_base);
//    cdc_acm_init(0,USB_OTG_FS_BASE);
//    cdc_acm_init(0,USB_OTG_HS_BASE);
    while(1){
//        cdc_acm_data_send_with_dtr_test();
    chry_dap_handle();
    chry_dap_usb2uart_handle();

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
    tx_byte_allocate(&byte_pool_0, (VOID **) &InitTaskPtr, DEMO_STACK_SIZE, TX_NO_WAIT);

    /* Create the main thread.  */
    tx_thread_create(&thread_0, "thread 0", thread_0_entry, 0,  
            pointer, DEMO_STACK_SIZE, 
            0, 0, TX_NO_TIME_SLICE, TX_AUTO_START);
    
     tx_thread_create(&ThreadInit, "thread Init", InitThread, 0,  
            InitTaskPtr, DEMO_STACK_SIZE, 
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
//    FPU_Init();
    bsp_sysclk_init();
    GpioLedInit();
    bspInitUart(115200);
    bsp_uart1_init(115200);
    UsartWirte(DEBUG_COM, (uint8_t*)g_hello, strlen(g_hello));
    
    
    
//    bsp_uart1_send((uint8_t*)g_hello, strlen(g_hello));
    // int* ptr = malloc(100);
    // if (ptr == NULL) {
    //     printf("malloc failed\r\n");
    //     while (1) {
    //     }
    // }
    // *ptr = 100;
    printf("Hello, world!\r\n");
    tx_kernel_enter();

    while (1)
    {
    }
}

/**@} end of group Template_Functions */
/**@} end of group Template */
/**@} end of group Examples */
