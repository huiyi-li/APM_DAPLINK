/*
 * MCU boot stages for the boot animation.
 */

#include "app_ui.h"

const app_boot_stage_t *app_boot_stages(int *count)
{
    static const app_boot_stage_t stages[] = {
        { "SPI flash",   30 },
        { "Filesystem",  60 },
        { "DAPLink",     85 },
        { "UI ready",   100 },
    };

    *count = (int)(sizeof(stages) / sizeof(stages[0]));
    return stages;
}
