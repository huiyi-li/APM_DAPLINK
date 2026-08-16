#include "filex_demo.h"

#include <stdio.h>
#include "bsp_printf.h"
#include "bsp_w25qxx.h"
#include <string.h>

#include "fx_api.h"
#include "fx_spi_flash_driver.h"

extern const uint8_t  algo_gd32f470_bin[];
extern const uint32_t algo_gd32f470_bin_len;
extern const char     algo_gd32f470_cfg[];
#include "tx_api.h"

/*
 * FileX demo on the W25Q128 SPI NOR flash.
 *
 * Boot sequence (low-priority thread, ~1s after system start):
 *   1. fx_system_initialize
 *   2. fx_media_open: on FX_MEDIA_INVALID, format the volume first
 *   3. create TEST.TXT with a repeating pattern, read back, verify
 *   4. list the root directory
 *   5. report free space, then suspend forever
 *
 * Media memory budget:
 *   - media cache: FX_MAX_SECTOR_CACHE (16) x 512B sectors + FileX
 *     bookkeeping, allocated from this static array
 *   - the driver itself keeps two 4KB block caches (static in the
 *     driver module)
 */

/* 16 sectors of 512B + FileX cache bookkeeping overhead. */
#define FX_DEMO_MEDIA_MEMORY_SIZE      (5U * 1024U)

#define FX_DEMO_THREAD_STACK_SIZE      4096U
#define FX_DEMO_THREAD_PRIORITY        9U
#define FX_DEMO_THREAD_DELAY_TICKS     100U   /* 1s at 10ms tick */

#define FX_DEMO_FILE_NAME              "TEST.TXT"
#define FX_DEMO_VOLUME_NAME            "APM32DISK"
#define FX_DEMO_FILE_SIZE              (8U * 1024U)
#define FX_DEMO_CHUNK_SIZE             (1U * 1024U)

static FX_MEDIA  s_media;
static UCHAR     s_media_memory[FX_DEMO_MEDIA_MEMORY_SIZE] __attribute__((section(".ccmram")));
static TX_THREAD s_thread;

FX_MEDIA *filex_demo_get_media(void)
{
    return &s_media;
}

volatile int g_prov_algo_bin = -1;
volatile int g_prov_algo_cfg = -1;
volatile int g_prov_create_bin = -1;
volatile int g_prov_create_cfg = -1;
volatile int g_prov_open_bin = -1;
volatile int g_prov_open_cfg = -1;
volatile int g_demo_open_status = -1;
volatile int g_list_st = -99;
volatile int g_list_count = -1;
volatile char g_list_first[32];
static UCHAR     s_thread_stack[FX_DEMO_THREAD_STACK_SIZE] __attribute__((section(".ccmram")));

static void filex_demo_entry(ULONG thread_input);
volatile int s_format_status = -1;
static UINT filex_demo_format(void)
{
    return fx_media_format(&s_media, fx_spi_flash_driver, NULL,
                           s_media_memory, sizeof(s_media_memory),
                           FX_DEMO_VOLUME_NAME,
                           1U,                        /* number of FATs    */
                           512U,                      /* directory entries */
                           0U,                      /* hidden sectors: the MSC device IS the volume */
                           FX_SPI_FLASH_PARTITION_SECTORS,
                           FX_SPI_FLASH_SECTOR_SIZE,  /* bytes per sector  */
                           8U,                        /* sectors/cluster   */
                           16U,                       /* heads             */
                           32U);                      /* sectors/track     */
}

static void filex_demo_format_failed(UINT status)
{
    printf("[FileX] format failed, status 0x%02X\r\n", status);
}

static UINT filex_demo_open_media(void)
{
    UINT status;

    /* TEMP force format */
    status = filex_demo_format();
    if (status != FX_SUCCESS)
    {
        return status;
    }
    status = fx_media_open(&s_media, FX_SPI_FLASH_MEDIA_NAME,
                           fx_spi_flash_driver, NULL,
                           s_media_memory, sizeof(s_media_memory));
    if ((status == FX_MEDIA_INVALID) || (status == FX_BOOT_ERROR))
    {
        printf("[FileX] no filesystem (0x%02X), formatting...\r\n", status);
        status = filex_demo_format();
        if (status != FX_SUCCESS)
        {
            filex_demo_format_failed(status);
            return status;
        }
        status = fx_media_open(&s_media, FX_SPI_FLASH_MEDIA_NAME,
                               fx_spi_flash_driver, NULL,
                               s_media_memory, sizeof(s_media_memory));
    }
    if (status != FX_SUCCESS)
    {
        printf("[FileX] media open failed, status 0x%02X\r\n", status);
    }
    return status;
}

static UINT filex_demo_write_and_verify(void)
{
    static UCHAR write_buf[FX_DEMO_CHUNK_SIZE];
    static UCHAR read_buf[FX_DEMO_CHUNK_SIZE];
    FX_FILE file;
    UINT status;
    UINT mismatch = 0U;

    status = fx_file_create(&s_media, FX_DEMO_FILE_NAME);
    if ((status != FX_SUCCESS) && (status != FX_ALREADY_CREATED))
    {
        printf("[FileX] create failed, status 0x%02X\r\n", status);
        return status;
    }

    status = fx_file_open(&s_media, &file, FX_DEMO_FILE_NAME, FX_OPEN_FOR_WRITE);
    if (status != FX_SUCCESS)
    {
        printf("[FileX] open(write) failed, status 0x%02X\r\n", status);
        return status;
    }
    for (ULONG chunk = 0U; chunk < FX_DEMO_FILE_SIZE / FX_DEMO_CHUNK_SIZE; ++chunk)
    {
        for (ULONG i = 0U; i < FX_DEMO_CHUNK_SIZE; ++i)
        {
            write_buf[i] = (UCHAR)((chunk * FX_DEMO_CHUNK_SIZE + i) * 7U + 3U);
        }
        status = fx_file_write(&file, write_buf, FX_DEMO_CHUNK_SIZE);
        if (status != FX_SUCCESS)
        {
            printf("[FileX] write failed, status 0x%02X\r\n", status);
            break;
        }
    }
    (void)fx_file_close(&file);
    if (status != FX_SUCCESS)
    {
        return status;
    }

    status = fx_file_open(&s_media, &file, FX_DEMO_FILE_NAME, FX_OPEN_FOR_READ);
    if (status != FX_SUCCESS)
    {
        printf("[FileX] open(read) failed, status 0x%02X\r\n", status);
        return status;
    }
    for (ULONG chunk = 0U; chunk < FX_DEMO_FILE_SIZE / FX_DEMO_CHUNK_SIZE; ++chunk)
    {
        ULONG actual = 0U;

        status = fx_file_read(&file, read_buf, FX_DEMO_CHUNK_SIZE, &actual);
        if (status != FX_SUCCESS)
        {
            printf("[FileX] read failed, status 0x%02X\r\n", status);
            break;
        }
        if (actual != FX_DEMO_CHUNK_SIZE)
        {
            printf("[FileX] short read: %lu/%lu\r\n",
                   (unsigned long)actual, (unsigned long)FX_DEMO_CHUNK_SIZE);
            status = FX_IO_ERROR;
            break;
        }
        for (ULONG i = 0U; i < FX_DEMO_CHUNK_SIZE; ++i)
        {
            if (read_buf[i] != (UCHAR)((chunk * FX_DEMO_CHUNK_SIZE + i) * 7U + 3U))
            {
                ++mismatch;
            }
        }
    }
    printf("[FileX] TEST.TXT %lu bytes, read back: %s (%u mismatches)\r\n",
           (unsigned long)FX_DEMO_FILE_SIZE,
           (mismatch == 0U) ? "OK" : "FAIL", mismatch);
    (void)fx_file_close(&file);
    return (status == FX_SUCCESS) ? FX_SUCCESS : status;
}

static void filex_demo_list_directory(void)
{
    CHAR name[FX_MAX_LAST_NAME_LEN + 1U];
    UINT status;

    status = fx_directory_first_entry_find(&s_media, name);
    if (status != FX_SUCCESS)
    {
        if (status != FX_NO_MORE_ENTRIES)
        {
            printf("[FileX] directory list failed, status 0x%02X\r\n", status);
        }
        return;
    }
    printf("[FileX] directory:\r\n");
    while (status == FX_SUCCESS)
    {
        printf("  %s\r\n", name);
        status = fx_directory_next_entry_find(&s_media, name);
    }
}

static void filex_demo_report(void)
{
    ULONG available;
    UINT status;

    status = fx_media_space_available(&s_media, &available);
    printf("[FileX] media ready, free space: %lu KB / %lu KB\r\n",
           (unsigned long)(available / 1024U),
           (unsigned long)(FX_SPI_FLASH_PARTITION_SECTORS *
                           FX_SPI_FLASH_SECTOR_SIZE / 1024U));
    (void)status;
}

static void filex_demo_provision_algo(FX_MEDIA *media)
{
    FX_FILE file;

    g_prov_algo_bin = -1;
    g_prov_algo_cfg = -1;

    /* algo code */
    if (fx_file_open(media, &file, "GD32F470_algo.bin", FX_OPEN_FOR_READ)
        != FX_SUCCESS)
    {
        (void)fx_file_delete(media, "GD32F470_algo.bin");
        g_prov_create_bin = (int)fx_file_create(media, "GD32F470_algo.bin");
        g_prov_open_bin = (int)fx_file_open(media, &file, "GD32F470_algo.bin",
                                            FX_OPEN_FOR_WRITE);
        if (g_prov_open_bin == FX_SUCCESS)
        {
            (void)fx_file_write(&file, (void *)algo_gd32f470_bin,
                                algo_gd32f470_bin_len);
            (void)fx_file_close(&file);
            g_prov_algo_bin = 1;
        }
    }
    else
    {
        (void)fx_file_close(&file);
        g_prov_algo_bin = 2;
    }

    /* cfg */
    if (fx_file_open(media, &file, "GD32F470.cfg", FX_OPEN_FOR_READ)
        != FX_SUCCESS)
    {
        (void)fx_file_delete(media, "GD32F470.cfg");
        g_prov_create_cfg = (int)fx_file_create(media, "GD32F470.cfg");
        g_prov_open_cfg = (int)fx_file_open(media, &file, "GD32F470.cfg",
                                            FX_OPEN_FOR_WRITE);
        if (g_prov_open_cfg == FX_SUCCESS)
        {
            (void)fx_file_write(&file, (void *)algo_gd32f470_cfg,
                                (ULONG)strlen(algo_gd32f470_cfg));
            (void)fx_file_close(&file);
            g_prov_algo_cfg = 1;
        }
    }
    else
    {
        (void)fx_file_close(&file);
        g_prov_algo_cfg = 2;
    }
}

static void filex_demo_entry(ULONG thread_input)
{
    UINT status;

    (void)thread_input;

    tx_thread_sleep(FX_DEMO_THREAD_DELAY_TICKS);
    printf("[FileX] demo starting (ThreadX tick %lu)\r\n",
           (unsigned long)tx_time_get());

    /* Full demo: init, open, format if needed, files, list, report. */
    fx_system_initialize();

    status = filex_demo_open_media();
    g_demo_open_status = (int)status;
    if (status == FX_SUCCESS)
    {
        /* provision the GD32F470 algorithm files into the volume root */
        filex_demo_provision_algo(&s_media);
        (void)filex_demo_write_and_verify();
        {
            UINT attr2;
            ULONG sz2;
            UINT y2, m2, d2, h2, mi2, s2;
            int cnt = 0;
            UINT st2 = fx_directory_first_full_entry_find(&s_media, "\\",
                                                          &attr2, &sz2,
                                                          &y2, &m2, &d2,
                                                          &h2, &mi2, &s2);
            g_list_st = (int)st2;
            if (st2 == FX_SUCCESS)
            {
                snprintf((char *)g_list_first, sizeof(g_list_first), "%s",
                         s_media.fx_media_name_buffer);
            }
            while (st2 == FX_SUCCESS)
            {
                cnt++;
                st2 = fx_directory_next_full_entry_find(&s_media, "\\",
                                                        &attr2, &sz2, &y2,
                                                        &m2, &d2, &h2, &mi2,
                                                        &s2);
            }
            g_list_count = cnt;
        }
        filex_demo_list_directory();
        filex_demo_report();
        (void)fx_media_close(&s_media);
    }
    printf("[FileX] demo done, status 0x%02X\r\n", status);

    /* Demo finished; suspend forever. */
    tx_thread_suspend(tx_thread_identify());
}

void filex_demo_start(void)
{
    UINT status;

    status = tx_thread_create(&s_thread, "filex demo", filex_demo_entry, 0,
                              s_thread_stack, sizeof(s_thread_stack),
                              FX_DEMO_THREAD_PRIORITY, 4,
                              TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS)
    {
        printf("[FileX] thread create failed 0x%02X\r\n", status);
    }
}
