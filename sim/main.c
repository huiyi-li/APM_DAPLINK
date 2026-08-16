/*
 * PC simulator for the DAPLink tool UI.
 *
 * Window: 240x240 logical, 3x zoom (720x720 physical).
 * Keys:   Up/Down = navigate, Enter = confirm, Esc = back (long-press OK on MCU).
 *         Mouse also works.
 *
 * Keyboard events are routed through sim_key.c, which translates them to
 * the same LV_KEY_* codes the MCU keypad indev will use (ui/app_keys.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "app_ui.h"

static void log_cb(lv_log_level_t level, const char * buf)
{
    (void)level;
    printf("[lv] %s\n", buf);
}

void sim_key_init(void);
void sim_key_poll_sdl(void);
void sim_autotest_start(void);

extern const char *sim_disk_root;

int main(int argc, char **argv)
{
    if(argc > 1) sim_disk_root = argv[1];
    printf("sim: disk root = %s\n", sim_disk_root);

    lv_init();
    lv_log_register_print_cb(log_cb);

    lv_display_t *disp = lv_sdl_window_create(240, 240);
    lv_sdl_window_set_zoom(disp, 3.0f);

    lv_indev_t *mouse = lv_sdl_mouse_create();
    (void)mouse;

    sim_key_init();

    app_ui_init();
    sim_autotest_start();

    while(1) {
        sim_key_poll_sdl();
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
