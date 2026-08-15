#include "fx_spi_flash_driver.h"

#include <stdbool.h>
#include <string.h>

#include "apm32f4xx.h"
#include "bsp_w25qxx.h"

/*
 * FileX driver for the W25Q128 SPI NOR flash.
 *
 * Layout: partition offset (1MB) + FAT volume (512B logical sectors).
 * W25Q erase granularity is 4KB, so writes go through a two-slot
 * 4KB block cache: a 512B sector update is applied to a cached block,
 * and the whole block is erased + rewritten on flush. This avoids
 * write amplification and erase churn on FAT metadata updates.
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

static uint32_t fx_spi_flash_logical_sector(FX_MEDIA *media_ptr)
{
    return media_ptr->fx_media_driver_logical_sector +
           media_ptr->fx_media_hidden_sectors;
}

static uint32_t fx_spi_flash_physical_address(FX_MEDIA *media_ptr)
{
    return FX_SPI_FLASH_PARTITION_OFFSET +
           (fx_spi_flash_logical_sector(media_ptr) *
            (uint32_t)media_ptr->fx_media_bytes_per_sector);
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

static void fx_spi_flash_writeback_slot(FX_MEDIA *media_ptr, uint32_t slot);

/* Pick a cache slot to evict: prefer an unused one, then a clean one,
 * otherwise flush the first slot. */
static uint32_t fx_spi_flash_cache_pick_slot(FX_MEDIA *media_ptr)
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
    fx_spi_flash_writeback_slot(media_ptr, 0U);
    return 0U;
}

/* Load a physical 4KB block into a cache slot. */
static bool fx_spi_flash_cache_load(FX_MEDIA *media_ptr, uint32_t slot,
                                    uint32_t block)
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
    (void)media_ptr;
    return true;
}

/* Write a dirty cache slot back to flash: erase the 4KB block, then
 * program it (bsp_w25qxx_write handles page boundaries). */
static void fx_spi_flash_writeback_slot(FX_MEDIA *media_ptr, uint32_t slot)
{
    const uint32_t address = FX_SPI_FLASH_PARTITION_OFFSET +
                             (s_cache[slot].block * FX_SPI_FLASH_BLOCK_SIZE);
    BSP_W25QXX_STATUS_T status;

    if (!s_cache[slot].valid || !s_cache[slot].dirty)
    {
        return;
    }
    status = bsp_w25qxx_erase_sector(address);
    if (status == BSP_W25QXX_OK)
    {
        status = bsp_w25qxx_write(address, s_cache[slot].data,
                                  FX_SPI_FLASH_BLOCK_SIZE);
    }
    if (status != BSP_W25QXX_OK)
    {
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        return;
    }
    s_cache[slot].dirty = false;
}

/* Ensure the block containing a logical sector is cached; returns the
 * slot index or -1 on error. */
static int32_t fx_spi_flash_cache_ensure(FX_MEDIA *media_ptr,
                                         uint32_t sector, uint32_t *slot)
{
    const uint32_t block = sector / FX_SPI_FLASH_SECTORS_PER_BLOCK;

    if (fx_spi_flash_cache_hit(block, slot))
    {
        return 0;
    }
    {
        const uint32_t pick = fx_spi_flash_cache_pick_slot(media_ptr);

        if (!fx_spi_flash_cache_load(media_ptr, pick, block))
        {
            return -1;
        }
        *slot = pick;
    }
    return 0;
}

static void fx_spi_flash_sector_read(FX_MEDIA *media_ptr)
{
    const uint32_t sector = fx_spi_flash_logical_sector(media_ptr);
    const uint32_t offset_in_block = (sector % FX_SPI_FLASH_SECTORS_PER_BLOCK) *
                                     (uint32_t)media_ptr->fx_media_bytes_per_sector;
    uint32_t slot;
    const size_t size = (size_t)media_ptr->fx_media_driver_sectors *
                        (size_t)media_ptr->fx_media_bytes_per_sector;

    if (fx_spi_flash_cache_ensure(media_ptr, sector, &slot) != 0)
    {
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        return;
    }
    (void)memcpy(media_ptr->fx_media_driver_buffer,
                 &s_cache[slot].data[offset_in_block], size);
    media_ptr->fx_media_driver_status = FX_SUCCESS;
}

static void fx_spi_flash_sector_write(FX_MEDIA *media_ptr)
{
    const uint32_t sector = fx_spi_flash_logical_sector(media_ptr);
    const uint32_t offset_in_block = (sector % FX_SPI_FLASH_SECTORS_PER_BLOCK) *
                                     (uint32_t)media_ptr->fx_media_bytes_per_sector;
    uint32_t slot;
    const size_t size = (size_t)media_ptr->fx_media_driver_sectors *
                        (size_t)media_ptr->fx_media_bytes_per_sector;

    if (fx_spi_flash_cache_ensure(media_ptr, sector, &slot) != 0)
    {
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        return;
    }
    (void)memcpy(&s_cache[slot].data[offset_in_block],
                 media_ptr->fx_media_driver_buffer, size);
    s_cache[slot].dirty = true;
    media_ptr->fx_media_driver_status = FX_SUCCESS;
}

static void fx_spi_flash_flush(FX_MEDIA *media_ptr)
{
    for (uint32_t i = 0U; i < FX_SPI_FLASH_CACHE_SLOTS; ++i)
    {
        fx_spi_flash_writeback_slot(media_ptr, i);
    }
    media_ptr->fx_media_driver_status = FX_SUCCESS;
}

static void fx_spi_flash_boot_read(FX_MEDIA *media_ptr)
{
    /* Boot sector is the first 512 bytes of the partition. */
    const uint32_t address = FX_SPI_FLASH_PARTITION_OFFSET;
    uint8_t *boot = media_ptr->fx_media_driver_buffer;

    if (bsp_w25qxx_read(address, boot, FX_SPI_FLASH_SECTOR_SIZE) != BSP_W25QXX_OK)
    {
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        return;
    }

    /* Validate the boot signature (jump instruction). */
    if ((boot[0] != (UCHAR)0xEB) ||
        ((boot[1] != (UCHAR)0x34) && (boot[1] != (UCHAR)0x76)) ||
        (boot[2] != (UCHAR)0x90))
    {
        media_ptr->fx_media_driver_status = FX_MEDIA_INVALID;
        return;
    }

    /* The cached sector size must fit the caller's buffer. */
    {
        const UINT bytes_per_sector = (UINT)((boot[11] << 8) | boot[12]);

        if (bytes_per_sector > media_ptr->fx_media_memory_size)
        {
            media_ptr->fx_media_driver_status = FX_BUFFER_ERROR;
            return;
        }
    }
    media_ptr->fx_media_driver_status = FX_SUCCESS;
}

static void fx_spi_flash_boot_write(FX_MEDIA *media_ptr)
{
    const uint32_t sector = fx_spi_flash_logical_sector(media_ptr);
    uint32_t slot;
    const uint32_t offset_in_block = (sector % FX_SPI_FLASH_SECTORS_PER_BLOCK) *
                                     (uint32_t)media_ptr->fx_media_bytes_per_sector;

    if (fx_spi_flash_cache_ensure(media_ptr, sector, &slot) != 0)
    {
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        return;
    }
    (void)memcpy(&s_cache[slot].data[offset_in_block],
                 media_ptr->fx_media_driver_buffer,
                 (size_t)media_ptr->fx_media_bytes_per_sector);
    s_cache[slot].dirty = true;
    fx_spi_flash_writeback_slot(media_ptr, slot);
    media_ptr->fx_media_driver_status = FX_SUCCESS;
}

void fx_spi_flash_driver(FX_MEDIA *media_ptr)
{
    switch (media_ptr->fx_media_driver_request)
    {
    case FX_DRIVER_INIT:
        fx_spi_flash_cache_reset();
        if (bsp_w25qxx_init() != BSP_W25QXX_OK)
        {
            media_ptr->fx_media_driver_status = FX_IO_ERROR;
            break;
        }
        media_ptr->fx_media_driver_free_sector_update = FX_FALSE;
        media_ptr->fx_media_driver_write_protect = FX_FALSE;
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    case FX_DRIVER_READ:
        fx_spi_flash_sector_read(media_ptr);
        break;

    case FX_DRIVER_WRITE:
        fx_spi_flash_sector_write(media_ptr);
        break;

    case FX_DRIVER_FLUSH:
        fx_spi_flash_flush(media_ptr);
        break;

    case FX_DRIVER_ABORT:
        fx_spi_flash_cache_reset();
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    case FX_DRIVER_BOOT_READ:
        fx_spi_flash_boot_read(media_ptr);
        break;

    case FX_DRIVER_BOOT_WRITE:
        fx_spi_flash_boot_write(media_ptr);
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
