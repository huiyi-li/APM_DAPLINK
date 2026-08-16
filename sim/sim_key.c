/*
 * Simulator keypad indev — the single place that maps PC keys to the
 * LV_KEY_* contract shared with the MCU (see ui/app_keys.h):
 *
 *   Up/Down  -> LV_KEY_PREV / LV_KEY_NEXT   (focus navigation)
 *   Enter    -> LV_KEY_ENTER                (confirm)
 *   Esc      -> LV_KEY_ESC                  (back; MCU: long-press OK)
 *
 * NOTE: LVGL 9 keypad navigation only reacts to NEXT/PREV, so arrows
 * must NOT be mapped to LV_KEY_UP/DOWN (those only scroll content).
 */
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "lvgl.h"
#include "app_keys.h"

#define KEYQ_SZ 16

static uint32_t keyq[KEYQ_SZ];
static int qh, qc;

static void keyq_push(uint32_t k)
{
    if(qc < KEYQ_SZ) {
        keyq[(qh + qc) % KEYQ_SZ] = k;
        qc++;
    }
}

static uint32_t keyq_pop(void)
{
    uint32_t k = keyq[qh];
    qh = (qh + 1) % KEYQ_SZ;
    qc--;
    return k;
}

static uint32_t sdl_to_lv(SDL_Keycode k)
{
    switch(k) {
    case SDLK_UP:      return APP_KEY_UP;     /* LV_KEY_PREV */
    case SDLK_DOWN:    return APP_KEY_DOWN;   /* LV_KEY_NEXT */
    case SDLK_RETURN:
    case SDLK_KP_ENTER:return APP_KEY_ENTER;
    case SDLK_ESCAPE:  return APP_KEY_BACK;
    default:           return 0;
    }
}

/*
 * LVGL keypad processing requires PRESSED/RELEASED alternation, otherwise
 * consecutive queued keys are dropped (PRESSED && prev==PRESSED is ignored).
 * So each queued key consumes two reads: PRESSED then RELEASED.
 */
static void kb_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;
    static uint32_t last_key = 0;
    static bool key_held = false;

    if(!key_held && qc > 0) {
        last_key = keyq_pop();
        key_held = true;
        lv_group_t *cg = app_keys_get_group();
        printf("[k] %u g=%p f=%p\n", last_key, (void *)cg,
               (void *)(cg ? lv_group_get_focused(cg) : NULL));
        fflush(stdout);
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = last_key;
    }
    else if(key_held) {
        key_held = false;
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
    }
    data->continue_reading = false;
}

/*
 * SDL event filter: intercepts key presses at the queue level, before ANY
 * SDL_PollEvent consumer (the LVGL window driver has its own event pump),
 * so there is no race between the two pumps.
 */
static int SDLCALL key_filter(void * userdata, SDL_Event * ev)
{
    (void)userdata;
    if(ev->type == SDL_KEYDOWN) {
        uint32_t k = sdl_to_lv(ev->key.keysym.sym);
        if(k) keyq_push(k);
        return 0;   /* consume: never seen by the LVGL window driver */
    }
    return 1;
}

static lv_indev_t *key_indev;
static lv_group_t *cur_group;

void app_keys_set_group(lv_group_t * g)
{
    cur_group = g;
    if(key_indev) lv_indev_set_group(key_indev, g);
}

lv_group_t * app_keys_get_group(void)
{
    return cur_group;
}

void sim_key_init(void)
{
    key_indev = lv_indev_create();
    lv_indev_set_display(key_indev, lv_display_get_default());
    lv_indev_set_type(key_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(key_indev, kb_read_cb);
    lv_indev_set_mode(key_indev, LV_INDEV_MODE_TIMER);

    SDL_SetEventFilter(key_filter, NULL);
}

/* no-op kept for API compatibility with the main loop */
void sim_key_poll_sdl(void)
{
}
