/* Host unit test for ui/app_algo.c (uses sim_fs.c against sim/disk). */
#include <stdio.h>
#include <string.h>
#include "app_algo.h"

const char *sim_disk_root = "sim/disk";

static void dump(void)
{
    app_algo_t list[APP_ALGO_MAX];
    int n = app_algo_load_all(list, APP_ALGO_MAX);
    printf("--- load_all -> %d ---\n", n);
    for(int i = 0; i < n; i++) {
        printf("  %-14s chip=%-12s flash=%4uKB ram=0x%08X def=%d bin=%s\n",
               list[i].name, list[i].chip, list[i].flash_size_kb, list[i].ram_addr,
               list[i].is_default, list[i].algo_bin);
    }
}

int main(void)
{
    int fail = 0;

    dump();

    app_algo_t a;
    if(app_algo_load("apm32f407", &a) != 0) { printf("FAIL load apm\n"); fail = 1; }
    else {
        printf("load apm32f407: chip=%s flash=%u ram=0x%08X default=%d init=0x%X page=%u\n",
               a.chip, a.flash_size_kb, a.ram_addr, a.is_default, a.fn_init, a.page_size);
        if(a.flash_size_kb != 1024 || a.ram_addr != 0x20000000 || !a.is_default) {
            printf("FAIL apm params\n"); fail = 1;
        }
        if(a.fn_init != 0x2C || a.fn_program_page != 0x84) {
            printf("FAIL fn offsets\n"); fail = 1;
        }
    }

    /* create gd32f407 (no cfg yet) */
    app_algo_t gd;
    memset(&gd, 0, sizeof(gd));
    snprintf(gd.name, sizeof(gd.name), "gd32f407");
    snprintf(gd.chip, sizeof(gd.chip), "GD32F407");
    snprintf(gd.algo_bin, sizeof(gd.algo_bin), "gd32f407_algo.bin");
    gd.flash_size_kb = 1024;
    gd.ram_addr = 0x20000000;
    gd.fn_init = 0x20; gd.fn_uninit = 0x34; gd.fn_erase_sector = 0x50; gd.fn_program_page = 0x78;
    gd.page_size = 2048; gd.sector_size = 4096;
    printf("save gd32f407: %d\n", app_algo_save(&gd));
    dump();

    printf("set_default gd32f407: %d\n", app_algo_set_default("gd32f407"));
    dump();
    if(app_algo_load("apm32f407", &a) == 0 && a.is_default) {
        printf("FAIL default not cleared on apm\n"); fail = 1;
    }
    if(app_algo_load("gd32f407", &a) == 0 && !a.is_default) {
        printf("FAIL default not set on gd\n"); fail = 1;
    }

    printf("delete stm32f103: %d\n", app_algo_delete("stm32f103"));
    dump();

    printf(fail ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fail;
}
