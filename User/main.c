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
#include "lcd_text.h"
#include "lvgl_port.h"
#include "lvgl_demo.h"
#include "app_ui.h"
#include "app_keys.h"
#include "filex_demo.h"
#include "fx_spi_flash_driver.h"



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
TX_THREAD               thread_lcd_test;
TX_THREAD               thread_lvgl;
TX_THREAD               thread_w25q_test;
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
TX_BYTE_POOL            usb_byte_pool;
TX_BLOCK_POOL           block_pool_0;

#define DEMO_STACK_SIZE         1024
#define DEMO_BYTE_POOL_SIZE     16384
#define USB_BYTE_POOL_SIZE      8192
#define DEMO_BLOCK_POOL_SIZE    100
#define DEMO_QUEUE_SIZE         100

#define LCD_TEST_STACK_SIZE     1024
#define LVGL_STACK_SIZE         8192

void thread_0_entry(ULONG thread_input)
{
    (void)thread_input;

    while(1)
    {
        const ULONG now = tx_time_get();

        bsp_button_process((uint32_t)now);
        tx_thread_sleep(10U);
    }
}

static const uint16_t lcd_test_colors[] = {
    DISPLAY_COLOR_RED,
    DISPLAY_COLOR_GREEN,
    DISPLAY_COLOR_BLUE,
    DISPLAY_COLOR_WHITE,
    DISPLAY_COLOR_BLACK,
};

/* ------------------------------------------------------------------ */
/* Button framework demo: show button state/events on the LCD.        */
/* ------------------------------------------------------------------ */

#define BTN_DISPLAY_COUNT 3U
#define BTN_STATE_COLOR_PRESS  DISPLAY_COLOR_YELLOW
#define BTN_STATE_COLOR_FREE   DISPLAY_COLOR_WHITE
#define BTN_EVENT_COLOR        DISPLAY_COLOR_CYAN
#define BTN_BG_COLOR           DISPLAY_COLOR_BLACK

typedef struct
{
    char state_text[20];
    char event_text[24];
    uint32_t event_serial;
} BTN_DISPLAY_T;

static BTN_DISPLAY_T s_btn_display[BTN_DISPLAY_COUNT];

/* Pads the string with spaces up to min_len so that redrawing always
 * overwrites any longer text that was previously displayed. */
static void btn_display_pad(char *buf, size_t buf_size, size_t min_len)
{
    size_t len = strlen(buf);

    while ((len < min_len) && (len + 1U < buf_size))
    {
        buf[len++] = ' ';
        buf[len] = '\0';
    }
}

static void btn_display_set_event(BSP_BUTTON_T button,
                                  BUTTON_EVENT_T event,
                                  uint32_t param)
{
    BTN_DISPLAY_T *disp;
    const char *name;

    if ((unsigned int)button >= BTN_DISPLAY_COUNT)
    {
        return;
    }
    disp = &s_btn_display[button];

    switch (event)
    {
        case BUTTON_EVENT_PRESSED:
            name = "PRESS";
            break;
        case BUTTON_EVENT_RELEASED:
            name = "RELEASE";
            break;
        case BUTTON_EVENT_CLICKED:
            name = "CLICK";
            break;
        case BUTTON_EVENT_DOUBLE_CLICKED:
            name = "2CLICK";
            break;
        case BUTTON_EVENT_TRIPLE_CLICKED:
            name = "3CLICK";
            break;
        case BUTTON_EVENT_MULTI_CLICKED:
            name = "MCLK";
            break;
        case BUTTON_EVENT_LONG_PRESSED:
            name = "LONG";
            break;
        case BUTTON_EVENT_EXTRA_LONG_PRESSED:
            name = "XLONG";
            break;
        default:
            name = "?";
            break;
    }

    if ((event == BUTTON_EVENT_CLICKED) ||
        (event == BUTTON_EVENT_DOUBLE_CLICKED) ||
        (event == BUTTON_EVENT_TRIPLE_CLICKED) ||
        (event == BUTTON_EVENT_MULTI_CLICKED))
    {
        (void)snprintf(disp->event_text, sizeof(disp->event_text), "K%lu>%s x%lu",
                       (unsigned long)(button + 1U), name, (unsigned long)param);
    }
    else
    {
        (void)snprintf(disp->event_text, sizeof(disp->event_text), "K%lu>%s",
                       (unsigned long)(button + 1U), name);
    }
    btn_display_pad(disp->event_text, sizeof(disp->event_text), 16U);
    disp->event_serial++;
}

static void btn_app_event_cb(BSP_BUTTON_T button,
                             BUTTON_EVENT_T event,
                             uint32_t param,
                             uint32_t now_ms,
                             void *user_data)
{
    (void)now_ms;
    (void)user_data;
    btn_display_set_event(button, event, param);
}

void thread_button_lcd_test_entry(ULONG thread_input)
{
    uint32_t last_event_serial[BTN_DISPLAY_COUNT] = { 0U, 0U, 0U, 0U };
    char line[24];
    const uint16_t row_state_y[BTN_DISPLAY_COUNT] = { 24U, 72U, 120U };
    const uint16_t row_event_y[BTN_DISPLAY_COUNT] = { 40U, 88U, 136U };

    (void)thread_input;

    while (!display_port_is_ready())
    {
        tx_thread_sleep(100U);
    }

    display_port_fill(&(DISPLAY_AREA_T){ 0U, 0U,
                                        DISPLAY_PORT_WIDTH - 1U,
                                        DISPLAY_PORT_HEIGHT - 1U },
                      BTN_BG_COLOR);
    lcd_text_draw_string(16U, 0U, "BUTTON TEST", DISPLAY_COLOR_WHITE, BTN_BG_COLOR);
    lcd_text_draw_string(16U, 156U, "SHORT/DOUBLE/TRIPLE", DISPLAY_COLOR_GREEN, BTN_BG_COLOR);
    lcd_text_draw_string(16U, 172U, "LONG/XLONG", DISPLAY_COLOR_GREEN, BTN_BG_COLOR);
    lcd_text_draw_string(16U, 188U, "CLICK=1 2CLK=2 3CLK=3", DISPLAY_COLOR_GREEN, BTN_BG_COLOR);
    printf("[BTN-LCD] test start\r\n");

    while (1)
    {
        for (uint32_t i = 0U; i < BTN_DISPLAY_COUNT; ++i)
        {
            const BSP_BUTTON_T button = (BSP_BUTTON_T)i;
            const uint32_t now = (uint32_t)tx_time_get();
            const uint32_t duration = bsp_button_press_duration_ms(button, now);
            const bool pressed = bsp_button_is_pressed(button);
            BTN_DISPLAY_T *disp = &s_btn_display[i];
            uint16_t state_color;

            /* State line (redrawn every cycle while held, to show ms). */
            if (pressed)
            {
                (void)snprintf(line, sizeof(line), "K%lu:PRESS %lu",
                               (unsigned long)(i + 1U), (unsigned long)duration);
                state_color = BTN_STATE_COLOR_PRESS;
            }
            else
            {
                (void)snprintf(line, sizeof(line), "K%lu:RELEASE",
                               (unsigned long)(i + 1U));
                state_color = BTN_STATE_COLOR_FREE;
            }
            btn_display_pad(line, sizeof(line), 14U);
            (void)lcd_text_draw_string(16U, row_state_y[i], line, state_color, BTN_BG_COLOR);

            /* Event line (only redrawn when a new event arrived). */
            if (disp->event_serial != last_event_serial[i])
            {
                last_event_serial[i] = disp->event_serial;
                (void)lcd_text_draw_string(16U, row_event_y[i], disp->event_text,
                                           BTN_EVENT_COLOR, BTN_BG_COLOR);
            }
        }
        tx_thread_sleep(100U);
    }
}

volatile uint32_t lvgl_heartbeat = 0U;

void thread_lvgl_entry(ULONG thread_input)
{
    (void)thread_input;

    while (!lvgl_port_init())
    {
        tx_thread_sleep(100U);
    }
    printf("[LVGL] port ready\r\n");
    app_keys_init();
    app_ui_init();
    /* lvgl_demo_start(); -- replaced by the app UI */


    while (1)
    {
        lvgl_port_handler();
        tx_thread_sleep(5U);
        lvgl_heartbeat++;
        if ((lvgl_heartbeat % 400U) == 0U)
        {
            printf("[LVGL] alive %lu\r\n", (unsigned long)(lvgl_heartbeat / 200U));
        }
    }
}

void thread_w25q_test_entry(ULONG thread_input);

void thread_lcd_test_entry(ULONG thread_input)
{
    static const DISPLAY_AREA_T full_screen = {
        0U, 0U, DISPLAY_PORT_WIDTH - 1U, DISPLAY_PORT_HEIGHT - 1U
    };
    uint32_t color_index = 0U;
    DISPLAY_PORT_STATUS_T status;

    (void)thread_input;

    /* Wait for the display driver to be initialized by InitThread. */
    while (!display_port_is_ready())
    {
        tx_thread_sleep(100U);
    }
    printf("[LCD] test start\r\n");

    while (1)
    {
        status = display_port_fill(&full_screen, lcd_test_colors[color_index]);
        printf("[LCD] fill #%lu color=0x%04X status=%d\r\n",
               (unsigned long)color_index,
               (unsigned int)lcd_test_colors[color_index],
               (int)status);

        color_index++;
        if (color_index >= (sizeof(lcd_test_colors) / sizeof(lcd_test_colors[0])))
        {
            color_index = 0U;
        }
        tx_thread_sleep(1000U);
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
    {
        static const DISPLAY_AREA_T full = { 0U, 0U, 239U, 239U };
        uint32_t t0, t1;

        /* full-screen fill timing */
        t0 = DWT->CYCCNT;
        (void)display_port_fill(&full, DISPLAY_COLOR_WHITE);
        t1 = DWT->CYCCNT;
        printf("[LCD] fullscreen fill: %lu ms\r\n",
               (unsigned long)((t1 - t0) / (SystemCoreClock / 1000U)));

        /* raw bus write timing: 1KB x 100 */
        {
            static uint8_t raw_buf[1024];
            #include "bsp_lcd_bus.h"
            for (uint32_t i = 0U; i < sizeof(raw_buf); ++i)
            {
                raw_buf[i] = (uint8_t)i;
            }
            t0 = DWT->CYCCNT;
            for (uint32_t i = 0U; i < 100U; ++i)
            {
                (void)bsp_lcd_bus_write(raw_buf, sizeof(raw_buf));
            }
            t1 = DWT->CYCCNT;
            printf("[LCD] 100x 1KB raw write: %lu ms (%lu us each)\r\n",
                   (unsigned long)((t1 - t0) / (SystemCoreClock / 1000U)),
                   (unsigned long)((t1 - t0) / (SystemCoreClock / 100000U)));
        }

        /* block flush timing (e.g. one row of text, 8x16 chars) */
        {
            uint16_t pixels[8U * 16U];
            DISPLAY_AREA_T area = { 0U, 24U, 7U, 39U };
            for (uint32_t i = 0U; i < 8U * 16U; ++i)
            {
                pixels[i] = (uint16_t)((i * 13U) & 0xFFFFU);
            }
            t0 = DWT->CYCCNT;
            for (uint32_t i = 0U; i < 100U; ++i)
            {
                (void)display_port_flush(&area, pixels, NULL, NULL);
            }
            t1 = DWT->CYCCNT;
            printf("[LCD] 100x 8x16 block flush: %lu ms (%lu us each)\r\n",
                   (unsigned long)((t1 - t0) / (SystemCoreClock / 1000U)),
                   (unsigned long)((t1 - t0) / (SystemCoreClock / 100000U)));
        }
    }
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
    CHAR    *LcdTestTaskPtr = TX_NULL;
    CHAR    *LvglTaskPtr = TX_NULL;
    CHAR    *W25QTaskPtr = TX_NULL;


    /* Create a byte memory pool from which to allocate the thread stacks.  */
    tx_byte_pool_create(&byte_pool_0, "byte pool 0", first_unused_memory, DEMO_BYTE_POOL_SIZE);
    tx_byte_pool_create(&usb_byte_pool, "usb byte pool", (VOID *)((uint32_t)first_unused_memory + DEMO_BYTE_POOL_SIZE), USB_BYTE_POOL_SIZE);
    fx_spi_flash_sys_init();

    /* Put system definition stuff in here, e.g. thread creates and other assorted
       create information.  */

    /* Allocate the stack for thread 0.  */
    tx_byte_allocate(&byte_pool_0, (VOID **) &pointer, DEMO_STACK_SIZE, TX_NO_WAIT);
    tx_byte_allocate(&byte_pool_0, (VOID **) &InitTaskPtr, 2048, TX_NO_WAIT);
    tx_byte_allocate(&byte_pool_0, (VOID **) &LcdTestTaskPtr, LCD_TEST_STACK_SIZE, TX_NO_WAIT);
    tx_byte_allocate(&byte_pool_0, (VOID **) &LvglTaskPtr, LVGL_STACK_SIZE, TX_NO_WAIT);

    /* Create the main thread.  */
    tx_thread_create(&thread_0, "thread 0", thread_0_entry, 0,
            pointer, DEMO_STACK_SIZE,
            0, 0, TX_NO_TIME_SLICE, TX_AUTO_START);

     tx_thread_create(&ThreadInit, "thread Init", InitThread, 0,
            InitTaskPtr, 2048,
            6, 4, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* LVGL demo thread (own 8KB stack). */
    tx_thread_create(&thread_lvgl, "thread lvgl", thread_lvgl_entry, 0,
            LvglTaskPtr, LVGL_STACK_SIZE,
            7, 4, TX_NO_TIME_SLICE, TX_AUTO_START);

    /* W25Q full test thread (low priority, after boot).
     * Disabled while investigating: W25Q shares SPI3 with the LCD, so
     * concurrent SPI use stalls LVGL rendering (arc hung at 36). */
    (void)W25QTaskPtr;
    (void)thread_w25q_test_entry;

    /* FileX demo thread (own static stack, low priority). */
    filex_demo_start();

    (void)thread_lcd_test;
    (void)thread_button_lcd_test_entry;

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
/* Full W25Q test runs in a low-priority thread after boot so it no
 * longer blocks system startup. */
void thread_w25q_test_entry(ULONG thread_input);

void thread_w25q_test_entry(ULONG thread_input)
{
    BSP_W25QXX_STATUS_T flash_status;
    uint32_t flash_jedec_id = 0U;
    uint8_t test_pattern[300];
    uint8_t read_back[300];
    static uint8_t perf_buf[4096];
    uint32_t t0, elapsed;
    uint32_t addr;
    const uint32_t perf_size = 256U * 1024U;

    (void)thread_input;

    tx_thread_sleep(2000U);

    printf("W25Q full test start\r\n");
    flash_status = bsp_w25qxx_init();
    if (flash_status != BSP_W25QXX_OK)
    {
        printf("W25Q init failed: %d\r\n", (int)flash_status);
        return;
    }
    (void)bsp_w25qxx_read_jedec_id(&flash_jedec_id);
    printf("W25Q JEDEC ID: %06lX\r\n", (unsigned long)flash_jedec_id);
    if (flash_jedec_id != BSP_W25QXX_JEDEC_ID)
    {
        printf("W25Q wrong chip (expected %06lX)\r\n",
               (unsigned long)BSP_W25QXX_JEDEC_ID);
        return;
    }
    printf("W25Q capacity: %lu bytes\r\n", (unsigned long)BSP_W25QXX_CAPACITY);

    printf("W25Q erase sector 0x000000...\r\n");
    flash_status = bsp_w25qxx_erase_sector(0x000000U);
    printf("W25Q erase: %s\r\n",
           (flash_status == BSP_W25QXX_OK) ? "ok" : "FAIL");

    for (uint32_t i = 0U; i < sizeof(test_pattern); ++i)
    {
        test_pattern[i] = (uint8_t)(i * 7U + 1U);
    }
    flash_status = bsp_w25qxx_write(0x000000U, test_pattern, sizeof(test_pattern));
    printf("W25Q write 300B (cross page): %s\r\n",
           (flash_status == BSP_W25QXX_OK) ? "ok" : "FAIL");

    flash_status = bsp_w25qxx_read(0x000000U, read_back, sizeof(read_back));
    if ((flash_status == BSP_W25QXX_OK) &&
        (memcmp(test_pattern, read_back, sizeof(test_pattern)) == 0))
    {
        printf("W25Q verify: PASS\r\n");
    }
    else
    {
        printf("W25Q verify: FAIL (%d)\r\n", (int)flash_status);
    }

    /* read 256KB throughput */
    for (uint32_t i = 0U; i < sizeof(perf_buf); ++i)
    {
        perf_buf[i] = (uint8_t)(i & 0xFFU);
    }
    t0 = DWT->CYCCNT;
    for (addr = 0x100000U; addr < 0x100000U + perf_size; addr += sizeof(perf_buf))
    {
        (void)bsp_w25qxx_read(addr, perf_buf, sizeof(perf_buf));
    }
    elapsed = (uint32_t)(DWT->CYCCNT - t0);
    {
        const uint32_t elapsed_ms = elapsed / (SystemCoreClock / 1000U);
        printf("W25Q read 256KB: %lu ms, %lu KB/s\r\n",
               (unsigned long)elapsed_ms,
               (unsigned long)((perf_size / 1024U) * 1000U / elapsed_ms));
    }

    /* erase + write 256KB throughput */
    {
        uint32_t erase_fail = 0U;
        uint32_t write_fail = 0U;
        uint32_t t_erase, t_write;

        t0 = DWT->CYCCNT;
        for (addr = 0x100000U; addr < 0x100000U + perf_size; addr += BSP_W25QXX_SECTOR_SIZE)
        {
            if (bsp_w25qxx_erase_sector(addr) != BSP_W25QXX_OK)
            {
                erase_fail++;
            }
        }
        t_erase = (uint32_t)(DWT->CYCCNT - t0);

        t0 = DWT->CYCCNT;
        for (addr = 0x100000U; addr < 0x100000U + perf_size; addr += sizeof(perf_buf))
        {
            for (uint32_t i = 0U; i < sizeof(perf_buf); ++i)
            {
                perf_buf[i] = (uint8_t)(addr + i);
            }
            if (bsp_w25qxx_write(addr, perf_buf, sizeof(perf_buf)) != BSP_W25QXX_OK)
            {
                write_fail++;
            }
        }
        t_write = (uint32_t)(DWT->CYCCNT - t0);

        printf("W25Q erase 256KB: %lu ms, write 256KB: %lu ms (erase_fail=%lu write_fail=%lu)\r\n",
               (unsigned long)(t_erase / (SystemCoreClock / 1000U)),
               (unsigned long)(t_write / (SystemCoreClock / 1000U)),
               (unsigned long)erase_fail,
               (unsigned long)write_fail);

        elapsed = (uint32_t)(DWT->CYCCNT - t0);
        {
            const uint32_t elapsed_ms = elapsed / (SystemCoreClock / 1000U);
            printf("W25Q erase+write 256KB: %lu ms, %lu KB/s\r\n",
                   (unsigned long)elapsed_ms,
                   (unsigned long)((perf_size / 1024U) * 1000U / elapsed_ms));
        }

        /* read-back verify of the performance region */
        {
            uint32_t verify_fail = 0U;
            for (addr = 0x100000U; addr < 0x100000U + perf_size; addr += sizeof(perf_buf))
            {
                (void)bsp_w25qxx_read(addr, perf_buf, sizeof(perf_buf));
                for (uint32_t i = 0U; i < sizeof(perf_buf); ++i)
                {
                    if (perf_buf[i] != (uint8_t)(addr + i))
                    {
                        verify_fail++;
                    }
                }
            }
            printf("W25Q perf region verify: %s (bad_bytes=%lu)\r\n",
                   (verify_fail == 0U) ? "PASS" : "FAIL",
                   (unsigned long)verify_fail);
        }
    }
    printf("W25Q full test done\r\n");
}

int main(void)
{
    uint32_t flash_jedec_id = 0U;

    bsp_sysclk_init();
    bsp_led_init();
    bsp_button_init();
    bsp_button_set_event_cb(btn_app_event_cb, NULL);
    bsp_debug_uart_init(115200U);
    (void)bsp_cdc_uart_init(NULL);
    bsp_uart1_init(115200U);
    bsp_debug_uart_write((const uint8_t *)g_hello, strlen(g_hello));
    printf("Hello, world!\r\n");

    /* Quick W25Q ID check only. */
    if (bsp_w25qxx_init() == BSP_W25QXX_OK)
    {
        (void)bsp_w25qxx_read_jedec_id(&flash_jedec_id);
        printf("W25Q JEDEC ID: %06lX\r\n", (unsigned long)flash_jedec_id);
    }
    else
    {
        printf("W25Q init failed\r\n");
    }
    tx_kernel_enter();

    while (1)
    {
    }
}

/**@} end of group Template_Functions */
/**@} end of group Template */
/**@} end of group Examples */
