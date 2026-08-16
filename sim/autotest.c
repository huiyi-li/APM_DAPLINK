/*
 * Optional automated UI walkthrough with BMP screenshots.
 * Enabled with: SIM_AUTOTEST=1 ./daplink_sim  (shots to $SIM_SHOT_DIR, default /tmp/opencode)
 *
 * Keys are injected through the REAL SDL event path (sim_key.c) when
 * SIM_REAL_KEYS=1, otherwise through lv_group_send_data. Both verify the
 * interaction flow; SIM_REAL_KEYS additionally exercises the indev chain.
 *
 * Script format per step: { "shot_name"|NULL, "keys", extra_wait_ticks }
 * Keys: U=Up D=Down E=Enter X=Esc (as SDL keysym / LV group keys).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#include "lvgl.h"
#include "src/others/snapshot/lv_snapshot.h"
#include "src/draw/lv_draw_buf.h"
#include "app_keys.h"

static const char *shot_dir = "/tmp/opencode";
static int real_keys = 0;

typedef struct {
    const char *shot;   /* screenshot name before acting, NULL = none */
    const char *keys;   /* "D E X U" sequence */
    int wait;           /* extra 900ms ticks to wait */
} astep_t;

static const astep_t script[] = {
    { "01_boot",            "",  3 },
    { "02_menu",            "D E", 0 },          /* Offline */
    { "03_flash_list",      "D D E", 0 },        /* .. -> ALGO CONFIG -> app.bin -> confirm */
    { "04_confirm",         "E",  0 },           /* Yes -> flashing */
    { "05_run_t0",          "",  0 },
    { "06_run_t1",          "",  2 },
    { "07_run_t2",          "",  2 },
    { "08_run_t3",          "",  2 },
    { "09_done",            "",  4 },
    { "10_result",          "X",  0 },
    { "11_list",            "",  0 },
};

static int step = 0;

static void shot(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.bmp", shot_dir, name);

    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if(!buf) {
        printf("snapshot failed: %s\n", name);
        return;
    }

    int w = buf->header.w, h = buf->header.h;
    uint32_t stride = (w * 3 + 3) & ~3;
    uint32_t bmp_size = 54 + stride * h;
    uint8_t *bmp = malloc(bmp_size);
    memset(bmp, 0, bmp_size);

    bmp[0] = 'B'; bmp[1] = 'M';
    *(uint32_t *)(bmp + 2) = bmp_size;
    *(uint32_t *)(bmp + 10) = 54;
    *(uint32_t *)(bmp + 14) = 40;
    *(int32_t *)(bmp + 18) = w;
    *(int32_t *)(bmp + 22) = h;
    *(uint16_t *)(bmp + 26) = 1;
    *(uint16_t *)(bmp + 28) = 24;

    const uint8_t *px = buf->data;
    for(int y = 0; y < h; y++) {
        uint8_t *dst = bmp + 54 + (h - 1 - y) * stride;
        for(int x = 0; x < w; x++) {
            uint32_t off = (y * buf->header.stride) + x * 4;
            /* LVGL ARGB8888 draw buffer memory order is {B,G,R,A} */
            dst[x * 3 + 0] = px[off + 0];
            dst[x * 3 + 1] = px[off + 1];
            dst[x * 3 + 2] = px[off + 2];
        }
    }

    FILE *f = fopen(path, "wb");
    if(f) { fwrite(bmp, 1, bmp_size, f); fclose(f); printf("shot: %s\n", path); }
    free(bmp);
    lv_draw_buf_destroy(buf);
}

static uint32_t key_to_sdl(char k)
{
    switch(k) {
    case 'U': return SDLK_UP;
    case 'D': return SDLK_DOWN;
    case 'E': return SDLK_RETURN;
    case 'X': return SDLK_ESCAPE;
    default: return 0;
    }
}

static uint32_t key_to_lv(char k)
{
    switch(k) {
    case 'U': return LV_KEY_PREV;
    case 'D': return LV_KEY_NEXT;
    case 'E': return LV_KEY_ENTER;
    case 'X': return LV_KEY_ESC;
    default: return 0;
    }
}

static void press_key(char k)
{
    if(real_keys) {
        SDL_Event ev = {0};
        ev.type = SDL_KEYDOWN;
        ev.key.keysym.sym = key_to_sdl(k);
        SDL_PushEvent(&ev);
    }
    else {
        /* emulate the keypad indev semantics */
        lv_group_t *g = app_keys_get_group();
        uint32_t lk = key_to_lv(k);
        lv_obj_t *f = lv_group_get_focused(g);
        if(lk == LV_KEY_NEXT) lv_group_focus_next(g);
        else if(lk == LV_KEY_PREV) lv_group_focus_prev(g);
        else if(lk == LV_KEY_ENTER) {
            if(f) lv_obj_send_event(f, LV_EVENT_CLICKED, NULL);
        }
        else {
            if(f) lv_obj_send_event(f, LV_EVENT_KEY, &lk);
        }
    }
}

static void script_step(lv_timer_t *timer)
{
    (void)timer;
    if(step >= (int)(sizeof(script) / sizeof(script[0]))) {
        lv_timer_delete(timer);
        printf("autotest done\n");
        exit(0);
    }

    const astep_t *st = &script[step];
    if(st->shot) shot(st->shot);

    for(const char *p = st->keys; *p; p++) {
        if(*p != ' ') press_key(*p);
    }
    step++;

    if(st->wait > 0) {
        lv_timer_set_period(timer, 900 * (1 + st->wait));
        lv_timer_reset(timer);
    }
    else {
        lv_timer_set_period(timer, 900);
        lv_timer_reset(timer);
    }
}

void sim_autotest_start(void)
{
    const char *env = getenv("SIM_AUTOTEST");
    if(!env || !atoi(env)) return;

    const char *dir = getenv("SIM_SHOT_DIR");
    if(dir) shot_dir = dir;
    mkdir(shot_dir, 0755);

    real_keys = atoi(getenv("SIM_REAL_KEYS") ? getenv("SIM_REAL_KEYS") : "0");

    printf("autotest: starting (shots -> %s, real keys: %d)\n", shot_dir, real_keys);
    lv_timer_t *t = lv_timer_create(script_step, 900, NULL);
    lv_timer_set_repeat_count(t, -1);
}
