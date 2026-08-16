#ifndef APP_KEYS_H
#define APP_KEYS_H

/*
 * Shared key event abstraction.
 *
 * Each screen owns its own lv_group; the keypad indev is switched to the
 * active screen's group on screen entry. This prevents focus from jumping
 * into objects of other (hidden) screens, which LVGL does not mark HIDDEN.
 *
 * Simulator (PC):   SDL keyboard -> keypad indev (sim/sim_key.c)
 *                   Up/Down/Enter/Esc -> APP_KEY_UP/DOWN/ENTER/BACK
 * MCU (3 buttons):  custom keypad indev feeds the same LV_KEY_* codes:
 *                   short press = ENTER, long press = BACK,
 *                   Up/Down buttons = APP_KEY_UP/DOWN.
 */

/* NOTE: LVGL 9 keypad focus navigation only reacts to NEXT/PREV (Tab semantics);
 * LV_KEY_UP/DOWN are sent to the focused widget for scrolling only. */
#define APP_KEY_UP      LV_KEY_PREV
#define APP_KEY_DOWN    LV_KEY_NEXT
#define APP_KEY_ENTER   LV_KEY_ENTER
#define APP_KEY_BACK    LV_KEY_ESC

/* Platform: bind the keypad indev to the active screen's group. */
void app_keys_set_group(lv_group_t * g);

/* Platform: current group bound to the keypad indev. */
lv_group_t * app_keys_get_group(void);

#endif
