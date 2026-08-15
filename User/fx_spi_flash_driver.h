#ifndef FX_SPI_FLASH_DRIVER_H
#define FX_SPI_FLASH_DRIVER_H

#include <stdint.h>

#include "fx_api.h"

/*
 * FileX driver for the W25Q128 SPI NOR flash (bsp_w25qxx).
 *
 * The flash is partitioned: the first FX_SPI_FLASH_PARTITION_OFFSET
 * bytes are reserved (unmanaged), the rest is a single FAT volume.
 * FileX logical sectors are 512 bytes; the W25Q physical erase block
 * is 4KB, so the driver caches whole 4KB blocks and performs
 * read-modify-write on demand (two block caches).
 *
 * The partition offset is communicated to FileX through the hidden
 * sectors field of the boot record (set at fx_media_format time), so
 * the media stays consistent with the physical layout.
 */

#define FX_SPI_FLASH_SECTOR_SIZE          512U
#define FX_SPI_FLASH_BLOCK_SIZE           4096U
#define FX_SPI_FLASH_SECTORS_PER_BLOCK    (FX_SPI_FLASH_BLOCK_SIZE / FX_SPI_FLASH_SECTOR_SIZE)

/* Reserved head space (kept for future use, e.g. DAPLink config). */
#define FX_SPI_FLASH_PARTITION_OFFSET     (1024U * 1024U)

#define FX_SPI_FLASH_TOTAL_BYTES          (16U * 1024U * 1024U)
#define FX_SPI_FLASH_PARTITION_SECTORS    ((FX_SPI_FLASH_TOTAL_BYTES - FX_SPI_FLASH_PARTITION_OFFSET) / FX_SPI_FLASH_SECTOR_SIZE)
#define FX_SPI_FLASH_HIDDEN_SECTORS       (FX_SPI_FLASH_PARTITION_OFFSET / FX_SPI_FLASH_SECTOR_SIZE)

/* Media name used with fx_media_open / fx_media_format. */
#define FX_SPI_FLASH_MEDIA_NAME           "W25Q128"

void fx_spi_flash_driver(FX_MEDIA *media_ptr);

/* LBA access shared by the MSC class (same volume FileX uses).
 * Returns 0 on success, non-zero on error. */
int fx_spi_flash_lba_read(uint32_t lba, void *buffer, uint32_t count);
int fx_spi_flash_lba_write(uint32_t lba, const void *buffer, uint32_t count);
uint32_t fx_spi_flash_lba_count(void);

/* Create the internal mutex (call once from tx_application_define). */
void fx_spi_flash_sys_init(void);

#endif
