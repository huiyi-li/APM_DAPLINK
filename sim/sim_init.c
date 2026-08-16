/*
 * Simulator boot stages: fake initialization progress shown on the boot screen.
 * MCU version will report real init points (FileX mount, USB enum, ...).
 */
#include "app_ui.h"

const char *sim_disk_root = "/home/ming/source/APM_DAPLINK/sim/disk";

static const app_boot_stage_t stages[] = {
    { "SYSTEM START",        5  },
    { "CLOCK & GPIO",        12 },
    { "MOUNT W25Q128",       22 },
    { "FILESYSTEM READY",    32 },
    { "USB ENUMERATING",     44 },
    { "CDC PORTS READY",     56 },
    { "HID DAP READY",       66 },
    { "MASS STORAGE READY",  76 },
    { "INPUT DRIVERS",       86 },
    { "UI LIBRARY",          94 },
    { "SYSTEM READY",        100},
};

const app_boot_stage_t *app_boot_stages(int *count)
{
    *count = (int)(sizeof(stages) / sizeof(stages[0]));
    return stages;
}
