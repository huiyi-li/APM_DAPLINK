#ifndef APP_UI_H
#define APP_UI_H

/*
 * Application UI: boot animation, main menu, DAPLink status,
 * file manager and offline programmer screens.
 *
 * Porting to MCU: implement app_fs/app_flash, feed the same LV_KEY_*
 * codes through a keypad indev into the UI group. UI code stays unchanged.
 */

/* Boot stage description, consumed by the boot screen animation. */
typedef struct {
    const char *label;
    int         pct;   /* progress percent at the end of this stage */
} app_boot_stage_t;

/* Called once after lv_init(). Creates all screens. */
void app_ui_init(void);

/* Boot stages for the current platform (sim -> sim/sim_init.c). */
const app_boot_stage_t *app_boot_stages(int *count);

/* Ask UI to switch to the main menu (e.g. boot finished). */
void app_ui_enter_menu(void);

#endif
