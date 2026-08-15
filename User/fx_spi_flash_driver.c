#include "fx_spi_flash_driver.h"

#include <stdbool.h>
#include <string.h>

#include "apm32f4xx.h"
#include "bsp_w25qxx.h"
#include "tx_api.h"

/*
 * FileX driver for the W25Q128 SPI NOR flash.
 *
 * Layout: partition offset (1MB) + FAT volume (512B logical sectors).
 * W25Q erase granularity is 4KB, so writes go through a block cache:
 * a 512B sector update is applied to a cached 4KB block, and the whole
 * block is erased + rewritten when it is evicted. This avoids write
 * amplification on FAT metadata updates.
 *
 * The same LBA view is exported (fx_spi_flash_lba_*) for the USB MSC
 * class so the host sees exactly the volume FileX formatted. All
 * access (FileX driver callbacks and MSC) is serialized by a mutex.
 */

/* Number of cached 4KB blocks. */
#define FX_SPI_FLASH_CACHE_SLOTS     1U

typedef struct
{
    bool     valid;                 /* cache line in use              */
    bool     dirty;                 /* modified since last writeback  */
    uint32_t block;                 /* physical 4KB block number      */
    uint8_t  data[FX_SPI_FLASH_BLOCK_SIZE];
} FX_SPI_FLASH_CACHE_T;

static FX_SPI_FLASH_CACHE_T s_cache[FX_SPI_FLASH_CACHE_SLOTS];
static TX_MUTEX s_mutex;
static bool     s_mutex_ready;


static void fx_spi_flash_lock(void)
{
    if (s_mutex_ready)
    {
        (void)tx_mutex_get(&s_mutex, TX_WAIT_FOREVER);
    }
}

static void fx_spi_flash_unlock(void)
{
    if (s_mutex_ready)
    {
        (void)tx_mutex_put(&s_mutex);
    }
}

static void fx_spi_flash_cache_reset(void)
{
    for (uint32_t i = 0U; i < FX_SPI_FLASH_CACHE_SLOTS; ++i)
    {
        s_cache[i].valid = false;
        s_cache[i].dirty = false;
    }
}

static bool fx_spi_flash_cache_hit(uint32_t block, uint32_t *slot)
{
    for (uint32_t i = 0U; i < FX_SPI_FLASH_CACHE_SLOTS; ++i)
    {
        if (s_cache[i].valid && (s_cache[i].block == block))
        {
            *slot = i;
            return true;
        }
    }
    return false;
}

/* Write a dirty cache slot back to flash: erase the 4KB block, then
 * program it (bsp_w25qxx_write handles page boundaries). */
static int fx_spi_flash_writeback_slot(uint32_t slot)
{
    const uint32_t address = FX_SPI_FLASH_PARTITION_OFFSET +
                             (s_cache[slot].block * FX_SPI_FLASH_BLOCK_SIZE);
    BSP_W25QXX_STATUS_T status;

    if (!s_cache[slot].valid || !s_cache[slot].dirty)
    {
        return 0;
    }
    status = bsp_w25qxx_erase_sector(address);
    if (status == BSP_W25QXX_OK)
    {
        status = bsp_w25qxx_write(address, s_cache[slot].data,
                                  FX_SPI_FLASH_BLOCK_SIZE);
    }
    if (status != BSP_W25QXX_OK)
    {
        return -1;
    }
    s_cache[slot].dirty = false;
    return 0;
}

/* Pick a cache slot to evict: prefer an unused one, then a clean one,
 * otherwise flush the first slot. */
static uint32_t fx_spi_flash_cache_pick_slot(void)
{
    uint32_t clean = FX_SPI_FLASH_CACHE_SLOTS;

    for (uint32_t i = 0U; i < FX_SPI_FLASH_CACHE_SLOTS; ++i)
    {
        if (!s_cache[i].valid)
        {
            return i;
        }
        if (!s_cache[i].dirty && (clean == FX_SPI_FLASH_CACHE_SLOTS))
        {
            clean = i;
        }
    }
    if (clean != FX_SPI_FLASH_CACHE_SLOTS)
    {
        return clean;
    }

    /* All slots dirty: write back slot 0 and reuse it. */
    (void)fx_spi_flash_writeback_slot(0U);
    return 0U;
}

/* Load a physical 4KB block into a cache slot. */
static bool fx_spi_flash_cache_load(uint32_t slot, uint32_t block)
{
    const uint32_t address = FX_SPI_FLASH_PARTITION_OFFSET +
                             (block * FX_SPI_FLASH_BLOCK_SIZE);

    if (bsp_w25qxx_read(address, s_cache[slot].data,
                        FX_SPI_FLASH_BLOCK_SIZE) != BSP_W25QXX_OK)
    {
        return false;
    }
    s_cache[slot].block = block;
    s_cache[slot].valid = true;
    s_cache[slot].dirty = false;
    return true;
}

/* Ensure the block containing a logical sector is cached; returns the
 * slot index or -1 on error. */
static int32_t fx_spi_flash_cache_ensure(uint32_t sector, uint32_t *slot)
{
    const uint32_t block = sector / FX_SPI_FLASH_SECTORS_PER_BLOCK;
    uint32_t pick;

    if (fx_spi_flash_cache_hit(block, slot))
    {
        return 0;
    }
    pick = fx_spi_flash_cache_pick_slot();
    if (!fx_spi_flash_cache_load(pick, block))
    {
        return -1;
    }
    *slot = pick;
    return 0;
}

/* Read one (or more contiguous) 512B sectors from the cache. */
static int fx_spi_flash_sector_read_raw(uint32_t sector, uint8_t *buffer,
                                        uint32_t count)
{
    while (count > 0U)
    {
        const uint32_t offset_in_block = (sector % FX_SPI_FLASH_SECTORS_PER_BLOCK) *
                                         FX_SPI_FLASH_SECTOR_SIZE;
        uint32_t slot;

        if (fx_spi_flash_cache_ensure(sector, &slot) != 0)
        {
            return -1;
        }
        (void)memcpy(buffer, &s_cache[slot].data[offset_in_block],
                     FX_SPI_FLASH_SECTOR_SIZE);
        ++sector;
        buffer += FX_SPI_FLASH_SECTOR_SIZE;
        --count;
    }
    return 0;
}

/* Write one (or more contiguous) 512B sectors through the cache. */
static int fx_spi_flash_sector_write_raw(uint32_t sector,
                                         const uint8_t *buffer,
                                         uint32_t count)
{
    while (count > 0U)
    {
        const uint32_t offset_in_block = (sector % FX_SPI_FLASH_SECTORS_PER_BLOCK) *
                                         FX_SPI_FLASH_SECTOR_SIZE;
        uint32_t slot;

        if (fx_spi_flash_cache_ensure(sector, &slot) != 0)
        {
            return -1;
        }
        (void)memcpy(&s_cache[slot].data[offset_in_block], buffer,
                     FX_SPI_FLASH_SECTOR_SIZE);
        s_cache[slot].dirty = true;
        ++sector;
        buffer += FX_SPI_FLASH_SECTOR_SIZE;
        --count;
    }
    return 0;
}

static void fx_spi_flash_flush_all(void)
{
    for (uint32_t i = 0U; i < FX_SPI_FLASH_CACHE_SLOTS; ++i)
    {
        (void)fx_spi_flash_writeback_slot(i);
    }
}

/* ------------------------------------------------------------------ */
/* LBA API shared with the USB MSC class                              */
/* ------------------------------------------------------------------ */

int fx_spi_flash_lba_read(uint32_t lba, void *buffer, uint32_t count)
{
    int result;

    if ((buffer == NULL) || (count == 0U) ||
        (lba + count > FX_SPI_FLASH_PARTITION_SECTORS))
    {
        return -1;
    }
    fx_spi_flash_lock();
    result = fx_spi_flash_sector_read_raw(lba, (uint8_t *)buffer, count);
    fx_spi_flash_unlock();
    return result;
}

int fx_spi_flash_lba_write(uint32_t lba, const void *buffer, uint32_t count)
{
    int result;

    if ((buffer == NULL) || (count == 0U) ||
        (lba + count > FX_SPI_FLASH_PARTITION_SECTORS))
    {
        return -1;
    }
    fx_spi_flash_lock();
    result = fx_spi_flash_sector_write_raw(lba, (const uint8_t *)buffer, count);
    fx_spi_flash_unlock();
    return result;
}

uint32_t fx_spi_flash_lba_count(void)
{
    return FX_SPI_FLASH_PARTITION_SECTORS;
}

void fx_spi_flash_sys_init(void)
{
    if (!s_mutex_ready)
    {
        if (tx_mutex_create(&s_mutex, "fx flash", TX_NO_INHERIT) == TX_SUCCESS)
        {
            s_mutex_ready = true;
        }
    }
}

/* ------------------------------------------------------------------ */
/* FileX driver callbacks                                             */
/* ------------------------------------------------------------------ */

void fx_spi_flash_driver(FX_MEDIA *media_ptr)
{
    const uint32_t sector = media_ptr->fx_media_driver_logical_sector;
    const uint32_t count = media_ptr->fx_media_driver_sectors;

    switch (media_ptr->fx_media_driver_request)
    {
    case FX_DRIVER_INIT:
        fx_spi_flash_lock();
        fx_spi_flash_cache_reset();
        if (bsp_w25qxx_init() != BSP_W25QXX_OK)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
        }
        else
        {
            media_ptr->fx_media_driver_free_sector_update = FX_FALSE;
            media_ptr->fx_media_driver_write_protect = FX_FALSE;
            media_ptr->fx_media_driver_status = FX_SUCCESS;
        }
        fx_spi_flash_unlock();
        break;

    case FX_DRIVER_READ:
        fx_spi_flash_lock();
        if (fx_spi_flash_sector_read_raw(sector, media_ptr->fx_media_driver_buffer,
                                         count) != 0)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
        }
        else
        {
            media_ptr->fx_media_driver_status = FX_SUCCESS;
        }
        fx_spi_flash_unlock();
        break;

    case FX_DRIVER_WRITE:
        fx_spi_flash_lock();
        if (fx_spi_flash_sector_write_raw(sector, media_ptr->fx_media_driver_buffer,
                                          count) != 0)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
        }
        else
        {
            media_ptr->fx_media_driver_status = FX_SUCCESS;
        }
        fx_spi_flash_unlock();
        break;

    case FX_DRIVER_FLUSH:
        fx_spi_flash_lock();
        fx_spi_flash_flush_all();
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        fx_spi_flash_unlock();
        break;

    case FX_DRIVER_ABORT:
        fx_spi_flash_lock();
        fx_spi_flash_cache_reset();
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        fx_spi_flash_unlock();
        break;

    case FX_DRIVER_BOOT_READ:
    {
        /* Boot sector is the first 512 bytes of the partition. */
        uint8_t *boot = media_ptr->fx_media_driver_buffer;

        fx_spi_flash_lock();
        if (bsp_w25qxx_read(FX_SPI_FLASH_PARTITION_OFFSET, boot,
                            FX_SPI_FLASH_SECTOR_SIZE) != BSP_W25QXX_OK)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
        }
        else
        {
            /* Validate the boot signature (jump instruction). */
            if ((boot[0] != (UCHAR)0xEB) ||
                ((boot[1] != (UCHAR)0x34) && (boot[1] != (UCHAR)0x76)) ||
                (boot[2] != (UCHAR)0x90))
            {
                media_ptr->fx_media_driver_status = FX_MEDIA_INVALID;
            }
            else
            {
                const UINT bytes_per_sector = (UINT)((boot[12] << 8) | boot[11]);

                if (bytes_per_sector > media_ptr->fx_media_memory_size)
                {
                    media_ptr->fx_media_driver_status = FX_BUFFER_ERROR;
                }
                else
                {
                    media_ptr->fx_media_driver_status = FX_SUCCESS;
                }
            }
        }
        fx_spi_flash_unlock();
        break;
    }

    case FX_DRIVER_BOOT_WRITE:
        fx_spi_flash_lock();
        if (fx_spi_flash_sector_write_raw(sector, media_ptr->fx_media_driver_buffer,
                                          count) != 0)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
        }
        else
        {
            fx_spi_flash_flush_all();
            media_ptr->fx_media_driver_status = FX_SUCCESS;
        }
        fx_spi_flash_unlock();
        break;

    case FX_DRIVER_RELEASE_SECTORS:
    case FX_DRIVER_UNINIT:
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    default:
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        break;
    }
}
