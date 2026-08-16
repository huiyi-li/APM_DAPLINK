#ifndef APP_ALGO_H
#define APP_ALGO_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Flash algorithm configuration.
 *
 * Each algorithm is stored as a text cfg file in /algo/:
 *   /algo/<name>.cfg     key=value, see app_algo.c
 *   /algo/<name>_algo.bin  raw Thumb code loaded into target RAM at ram_addr
 *
 * cfg files are produced on the PC by tools/flm2bin.py from Keil FLM files;
 * the config menu lists them, marks one as default, and the offline
 * programmer uses the default algorithm when flashing.
 */

#define APP_ALGO_NAME_MAX   32
#define APP_ALGO_CHIP_MAX   32
#define APP_ALGO_BIN_MAX    64
#define APP_ALGO_MAX        16

typedef struct {
    char     name[APP_ALGO_NAME_MAX];   /* cfg base name */
    char     chip[APP_ALGO_CHIP_MAX];   /* chip model */
    uint32_t flash_size_kb;
    uint32_t ram_addr;                  /* algorithm RAM base */
    char     algo_bin[APP_ALGO_BIN_MAX];
    bool     is_default;

    /* function offsets inside algo_bin (from FLM symbol table) */
    uint32_t fn_init;
    uint32_t fn_uninit;
    uint32_t fn_erase_chip;
    uint32_t fn_erase_sector;
    uint32_t fn_program_page;
    uint32_t page_size;
    uint32_t sector_size;
} app_algo_t;

/* Scan /algo/*.cfg into `list`. Returns number of entries, or -1 on error. */
int app_algo_load_all(app_algo_t *list, int max);

/* Write /algo/<name>.cfg. 0 on success. */
int app_algo_save(const app_algo_t *a);

/* Delete /algo/<name>.cfg. 0 on success. */
int app_algo_delete(const char *name);

/* Mark one algorithm as default (clears others). 0 on success. */
int app_algo_set_default(const char *name);

/* Load a single algorithm by name. 0 on success, -1 if not found. */
int app_algo_load(const char *name, app_algo_t *a);

#endif
