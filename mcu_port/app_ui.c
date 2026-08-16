/*
 * DAPLink tool UI — all screens and interaction logic.
 *
 * Input model (simulator == MCU): lv_group + keypad indev
 *   LV_KEY_UP / LV_KEY_DOWN : navigate
 *   LV_KEY_ENTER            : confirm
 *   LV_KEY_ESC              : back (MCU: long-press OK button)
 *
 * FS and flash engines are abstracted (ui/app_fs.h, ui/app_flash.h),
 * so this file compiles unchanged on the simulator and on the MCU.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lvgl.h"

/* portable case-insensitive compare (avoids strings.h on MCU) */
static int app_stricmp(const char *a, const char *b)
{
    while(*a && *b) {
        char ca = *a, cb = *b;
        if(ca >= 'A' && ca <= 'Z') ca += 32;
        if(cb >= 'A' && cb <= 'Z') cb += 32;
        if(ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
#include "app_ui.h"
#include "app_fs.h"
#include "app_flash.h"
#include "app_keys.h"
#include "app_algo.h"

#define SCR_W 240
#define SCR_H 240

#define HEADER_H 34
#define FOOTER_H 24

/*-------------------------- navigation ------------------------*/

typedef enum {
    SCR_MENU,
    SCR_DAP,
    SCR_FM,
    SCR_FLASH,
} scr_t;

static lv_obj_t *scr_boot, *scr_menu, *scr_dap, *scr_fm, *scr_flash;
static lv_group_t *g_menu, *g_dap, *g_fm, *g_flash;
static scr_t scr_stack[8];
static int scr_depth = 0;

/* Switch screens: load + bind indev to the screen's group + focus default. */
static void ui_enter_screen(lv_obj_t *scr, lv_group_t *g, lv_obj_t *first)
{
    lv_screen_load(scr);
    app_keys_set_group(g);
    if(first) lv_group_focus_obj(first);
}

static void push_screen(scr_t s)
{
    scr_stack[scr_depth++] = s;
}

static scr_t pop_screen(void)
{
    if(scr_depth > 0) scr_depth--;
    return scr_depth > 0 ? scr_stack[scr_depth - 1] : SCR_MENU;
}

static void menu_focus_first(void)
{
    for(int i = 1; i < 4; i++) {
        lv_obj_t *b = lv_obj_get_child(scr_menu, i);
        if(b && lv_obj_has_flag(b, LV_OBJ_FLAG_CLICKABLE)) {
            lv_group_focus_obj(b);
            return;
        }
    }
}

static void back_to_menu(void)
{
    pop_screen();
    ui_enter_screen(scr_menu, g_menu, lv_obj_get_child(scr_menu, 1));
}

/*-------------------------- helpers ---------------------------*/

static lv_obj_t *make_header(lv_obj_t *parent, const char *title)
{
    lv_obj_t *h = lv_obj_create(parent);
    lv_obj_remove_flag(h, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(h, SCR_W, HEADER_H);
    lv_obj_set_pos(h, 0, 0);
    lv_obj_set_style_bg_color(h, lv_color_hex(0x1e1e24), 0);
    lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(h, 0, 0);
    lv_obj_set_style_pad_all(h, 0, 0);

    lv_obj_t *t = lv_label_create(h);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0xdddddd), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_center(t);
    return h;
}

static lv_obj_t *make_footer(lv_obj_t *parent, const char *hint)
{
    lv_obj_t *f = lv_obj_create(parent);
    lv_obj_remove_flag(f, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(f, SCR_W, FOOTER_H);
    lv_obj_set_pos(f, 0, SCR_H - FOOTER_H);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(f, lv_color_hex(0x141418), 0);
    lv_obj_set_style_border_width(f, 0, 0);
    lv_obj_set_style_pad_all(f, 0, 0);

    lv_obj_t *t = lv_label_create(f);
    lv_label_set_text(t, hint);
    lv_obj_set_style_text_color(t, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
    lv_obj_center(t);
    return f;
}

static lv_obj_t *make_body(lv_obj_t *parent)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, SCR_W, SCR_H - HEADER_H - FOOTER_H);
    lv_obj_set_pos(b, 0, HEADER_H);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 4, 0);
    lv_obj_set_layout(b, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return b;
}

/* Row button: [name............... size]
 * Takes an ownership-free snapshot of `ud`: the entry data is copied so
 * the caller may free its entries array immediately. The copy is freed
 * when the row object is deleted. */
static void row_delete_cb(lv_event_t *e)
{
    app_fs_entry_t *p = lv_event_get_user_data(e);
    if(p) lv_free(p);
}

/* optional per-screen hook: called when any row gains focus */
static void (*row_focus_hook)(const app_fs_entry_t *ent);

static void row_focus_internal_cb(lv_event_t *e)
{
    if(row_focus_hook) row_focus_hook(lv_event_get_user_data(e));
}

static lv_obj_t *make_row(lv_obj_t *parent, lv_group_t *g, const char *name, const char *size_str,
                          const app_fs_entry_t *ud, lv_event_cb_t click_cb, lv_event_cb_t key_cb)
{
    app_fs_entry_t *copy = NULL;
    if(ud) {
        copy = lv_malloc(sizeof(app_fs_entry_t));
        if(copy) *copy = *ud;
    }

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_width(btn, SCR_W - 16);
    lv_obj_set_height(btn, 40);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(btn, 10, 0);
    lv_obj_set_style_pad_ver(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x26262e), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x30303c), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x34344a), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);

    lv_obj_t *n = lv_label_create(btn);
    lv_label_set_text(n, name);
    lv_obj_set_flex_grow(n, 1);
    lv_obj_set_style_text_color(n, lv_color_hex(0xe8e8e8), 0);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_14, 0);

    if(size_str) {
        lv_obj_t *s = lv_label_create(btn);
        lv_label_set_text(s, size_str);
        lv_obj_set_style_text_color(s, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
    }

    if(copy) {
        if(click_cb) lv_obj_add_event_cb(btn, click_cb, LV_EVENT_CLICKED, copy);
        if(key_cb) lv_obj_add_event_cb(btn, key_cb, LV_EVENT_KEY, copy);
        lv_obj_add_event_cb(btn, row_focus_internal_cb, LV_EVENT_FOCUSED, copy);
        lv_obj_add_event_cb(btn, row_delete_cb, LV_EVENT_DELETE, copy);
    }
    lv_group_add_obj(g, btn);
    return btn;
}

/*------------------------- modal dialog -----------------------*/

typedef enum {
    DLG_DELETE_FILE,
    DLG_DELETE_DIR,
    DLG_FLASH_CONFIRM,
    DLG_INFO,
    DLG_ALGO_DELETE,
} dlg_kind_t;

typedef struct {
    lv_obj_t *overlay;
    dlg_kind_t kind;
    char target[96];
    char yes_label[12];
} dlg_ctx_t;

static dlg_ctx_t dlg;
static lv_obj_t *dlg_prev_focus = NULL;

static void dlg_yes_cb(lv_event_t *e);
static void dlg_no_cb(lv_event_t *e);

static void dlg_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) == LV_KEY_ESC)
        dlg_no_cb(e);
}

static void show_dialog(dlg_kind_t kind, const char *msg, const char *target,
                        const char *yes_label)
{
    lv_obj_t *ov = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ov, SCR_W, SCR_H);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x0a0a0c), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_move_foreground(ov);

    lv_obj_t *box = lv_obj_create(ov);
    lv_obj_set_size(box, SCR_W - 32, 132);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x232329), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x3a3a44), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_pad_all(box, 10, 0);
    lv_obj_set_layout(box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 6, 0);
    lv_obj_center(box);

    lv_obj_t *m = lv_label_create(box);
    lv_label_set_text(m, msg);
    lv_obj_set_style_text_color(m, lv_color_hex(0xe8e8e8), 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_14, 0);
    lv_obj_set_width(m, lv_pct(100));
    lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, target);
    lv_obj_set_style_text_color(t, lv_color_hex(0xffc46b), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_set_width(t, lv_pct(100));
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(t, LV_OPA_50, 0);

    lv_obj_t *row = lv_obj_create(box);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);

    dlg.kind = kind;
    dlg.overlay = ov;
    snprintf(dlg.target, sizeof(dlg.target), "%s", target);
    snprintf(dlg.yes_label, sizeof(dlg.yes_label), "%s", yes_label);

    lv_obj_t *yes = lv_button_create(row);
    lv_obj_set_size(yes, 88, 34);
    lv_obj_set_style_bg_color(yes, lv_color_hex(0x1f5f2f), 0);
    lv_obj_set_style_bg_color(yes, lv_color_hex(0x2a8a40), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(yes, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(yes, 0, 0);
    lv_obj_set_style_shadow_width(yes, 0, 0);
    lv_obj_set_style_outline_width(yes, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(yes, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_obj_t *yl = lv_label_create(yes);
    lv_label_set_text(yl, yes_label);
    lv_obj_center(yl);
    lv_obj_add_event_cb(yes, dlg_yes_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(yes, dlg_key_cb, LV_EVENT_KEY, NULL);

    lv_obj_t *no = lv_button_create(row);
    lv_obj_set_size(no, 88, 34);
    lv_obj_set_style_bg_color(no, lv_color_hex(0x4a3a3a), 0);
    lv_obj_set_style_bg_color(no, lv_color_hex(0x5f4545), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(no, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(no, 0, 0);
    lv_obj_set_style_shadow_width(no, 0, 0);
    lv_obj_set_style_outline_width(no, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(no, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_obj_t *nl = lv_label_create(no);
    lv_label_set_text(nl, "NO");
    lv_obj_center(nl);
    lv_obj_add_event_cb(no, dlg_no_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(no, dlg_key_cb, LV_EVENT_KEY, NULL);

    lv_group_t *g = app_keys_get_group();
    dlg_prev_focus = lv_group_get_focused(g);
    lv_group_add_obj(g, yes);
    lv_group_add_obj(g, no);
    lv_group_focus_obj(yes);

    lv_obj_update_layout(box);
}

static void dialog_cleanup(void)
{
    if(!dlg.overlay) return;
    lv_obj_delete(dlg.overlay);
    dlg.overlay = NULL;
    if(dlg_prev_focus && !lv_obj_is_valid(dlg_prev_focus)) dlg_prev_focus = NULL;
    if(dlg_prev_focus) lv_group_focus_obj(dlg_prev_focus);
}

/*-------------------------- boot screen -----------------------*/

static lv_timer_t *boot_timer;
static int boot_idx;
static lv_obj_t *boot_bar, *boot_pct, *boot_label;

static void boot_tick(lv_timer_t *t)
{
    (void)t;
    int n;
    const app_boot_stage_t *st = app_boot_stages(&n);
    boot_idx++;
    if(boot_idx >= n) {
        lv_timer_delete(boot_timer);
        boot_timer = NULL;
        app_ui_enter_menu();
        return;
    }
    lv_bar_set_value(boot_bar, st[boot_idx].pct, LV_ANIM_OFF);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", st[boot_idx].pct);
    lv_label_set_text(boot_pct, buf);
    lv_label_set_text(boot_label, st[boot_idx].label);
}

static void create_boot_screen(void)
{
    scr_boot = lv_obj_create(NULL);
    lv_obj_set_size(scr_boot, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr_boot, lv_color_hex(0x141418), 0);
    lv_obj_set_style_bg_opa(scr_boot, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_boot, 0, 0);

    lv_obj_t *title = lv_label_create(scr_boot);
    lv_label_set_text(title, "DAPLINK TOOL");
    lv_obj_set_style_text_color(title, lv_color_hex(0x4fc3f7), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);

    lv_obj_t *sub = lv_label_create(scr_boot);
    lv_label_set_text(sub, "SYSTEM INITIALIZING");
    lv_obj_set_style_text_color(sub, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, -30);

    boot_bar = lv_bar_create(scr_boot);
    lv_obj_set_size(boot_bar, 200, 12);
    lv_obj_align(boot_bar, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_style_radius(boot_bar, 6, 0);
    lv_obj_set_style_bg_color(boot_bar, lv_color_hex(0x2a2a32), 0);
    lv_obj_set_style_bg_color(boot_bar, lv_color_hex(0x1f8e4e), LV_PART_INDICATOR);
    lv_bar_set_range(boot_bar, 0, 100);
    lv_bar_set_value(boot_bar, 0, LV_ANIM_OFF);

    boot_pct = lv_label_create(scr_boot);
    lv_label_set_text(boot_pct, "0%");
    lv_obj_set_style_text_color(boot_pct, lv_color_hex(0x1f8e4e), 0);
    lv_obj_set_style_text_font(boot_pct, &lv_font_montserrat_16, 0);
    lv_obj_align(boot_pct, LV_ALIGN_CENTER, 0, 16);

    boot_label = lv_label_create(scr_boot);
    lv_label_set_text(boot_label, "SYSTEM START");
    lv_obj_set_style_text_color(boot_label, lv_color_hex(0x777777), 0);
    lv_obj_align(boot_label, LV_ALIGN_CENTER, 0, 46);

    boot_idx = 0;
    boot_timer = lv_timer_create(boot_tick, 250, NULL);
}

/*-------------------------- main menu -------------------------*/

static void fm_refresh_path(const char *path);
static void flash_refresh_list(void);
static void dap_enter(void);
static lv_obj_t *fm_first_row(void);
static lv_obj_t *flash_first_row(void);

static void menu_item_cb(lv_event_t *e)
{
    scr_t target = (scr_t)(lv_event_get_user_data(e));
    if(target == SCR_DAP) {
        push_screen(SCR_DAP);
        dap_enter();
    } else if(target == SCR_FM) {
        fm_refresh_path("/");
        push_screen(SCR_FM);
        ui_enter_screen(scr_fm, g_fm, fm_first_row());
    } else if(target == SCR_FLASH) {
        flash_refresh_list();
        push_screen(SCR_FLASH);
        ui_enter_screen(scr_flash, g_flash, flash_first_row());
    }
}

static void create_menu_screen(void)
{
    scr_menu = lv_obj_create(NULL);
    lv_obj_set_size(scr_menu, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr_menu, lv_color_hex(0x141418), 0);
    lv_obj_set_style_bg_opa(scr_menu, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_menu, 0, 0);

    lv_obj_t *title = lv_label_create(scr_menu);
    lv_label_set_text(title, "DAPLINK TOOL");
    lv_obj_set_style_text_color(title, lv_color_hex(0x4fc3f7), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    g_menu = lv_group_create();
    const char *items[3] = { "DAPLink Mode", "Offline Programmer", "File Manager" };
    scr_t targets[3] = { SCR_DAP, SCR_FLASH, SCR_FM };

    for(int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(scr_menu);
        lv_obj_set_size(btn, SCR_W - 40, 44);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 58 + i * 52);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x232329), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x34344a), LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
        lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);

        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, items[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0xe8e8e8), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_center(l);

        lv_obj_add_event_cb(btn, menu_item_cb, LV_EVENT_CLICKED, (void *)(targets[i]));
        lv_group_add_obj(g_menu, btn);
    }

    make_footer(scr_menu, "UP/DOWN SELECT  OK ENTER");
}

/*-------------------------- DAPLink mode ----------------------*/

static lv_obj_t *dap_back_btn;

static void dap_enter(void)
{
    ui_enter_screen(scr_dap, g_dap, dap_back_btn);
}

static void dap_back_cb(lv_event_t *e)
{
    (void)e;
    back_to_menu();
}

static void dap_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    back_to_menu();
}

static void create_dap_screen(void)
{
    g_dap = lv_group_create();
    scr_dap = lv_obj_create(NULL);
    lv_obj_set_size(scr_dap, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr_dap, lv_color_hex(0x141418), 0);
    lv_obj_set_style_bg_opa(scr_dap, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_dap, 0, 0);

    make_header(scr_dap, "DAPLINK MODE");

    lv_obj_t *body = make_body(scr_dap);
    const char *status[] = {
        "USB CDC:     Connected",
        "USB HID:     Connected",
        "USB MSC:     Connected",
        "Firmware:    v1.0.0",
        "Target:      SWD / APM32",
    };
    for(unsigned i = 0; i < sizeof(status) / sizeof(status[0]); i++) {
        lv_obj_t *l = lv_label_create(body);
        lv_label_set_text(l, status[i]);
        lv_obj_set_style_text_color(l, i < 3 ? lv_color_hex(0x4caf50)
                                            : lv_color_hex(0xaaaaaa), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    }

    dap_back_btn = lv_button_create(body);
    lv_obj_set_width(dap_back_btn, 140);
    lv_obj_set_height(dap_back_btn, 38);
    lv_obj_set_style_bg_color(dap_back_btn, lv_color_hex(0x26262e), 0);
    lv_obj_set_style_bg_color(dap_back_btn, lv_color_hex(0x34344a), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(dap_back_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dap_back_btn, 0, 0);
    lv_obj_set_style_shadow_width(dap_back_btn, 0, 0);
    lv_obj_set_style_outline_width(dap_back_btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(dap_back_btn, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_obj_t *bl = lv_label_create(dap_back_btn);
    lv_label_set_text(bl, "BACK");
    lv_obj_center(bl);
    lv_obj_add_event_cb(dap_back_btn, dap_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(dap_back_btn, dap_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(g_dap, dap_back_btn);

    make_footer(scr_dap, "OK: BACK");
}

/*-------------------------- file manager ----------------------*/

static char fm_path[96] = "/";
static char fm_selected[128] = "";
static lv_obj_t *fm_list;
static lv_obj_t *fm_path_lbl;

static void fm_join(const char *name, char *out, size_t sz);

static void fm_row_focus_cb(const app_fs_entry_t *ent)
{
    if(!ent) return;
    if(strcmp(ent->name, "..") == 0) fm_selected[0] = '\0';
    else fm_join(ent->name, fm_selected, sizeof(fm_selected));
}

/* DELETE row: remove the currently selected entry (file or folder) */
static void fm_delete_cb(lv_event_t *e)
{
    (void)e;
    if(fm_selected[0] == '\0') return;
    bool is_dir = app_fs_is_dir(fm_selected);
    const char *msg = is_dir ? "DELETE FOLDER AND ALL" : "DELETE FILE?";
    const char *sub = is_dir ? "CONTENTS? (RECURSIVE)" : "";
    char full[160];
    snprintf(full, sizeof(full), "%s\n%s", msg, sub);
    show_dialog(is_dir ? DLG_DELETE_DIR : DLG_DELETE_FILE, full, fm_selected, "DELETE");
}

static void fm_focus_first(void);
static void fm_refresh_path(const char *path);
static void fm_show_delete_dlg(const char *path, bool is_dir);
static void fm_delete_cb(lv_event_t *e);
static void fm_join(const char *name, char *out, size_t sz);

static void fm_join(const char *name, char *out, size_t sz)
{
    if(strcmp(fm_path, "/") == 0) snprintf(out, sz, "/%s", name);
    else snprintf(out, sz, "%s/%s", fm_path, name);
}

static void fm_parent(char *out, size_t sz)
{
    char tmp[96];
    snprintf(tmp, sizeof(tmp), "%s", fm_path);
    char *slash = strrchr(tmp, '/');
    if(!slash || slash == tmp) snprintf(out, sz, "/");
    else { *slash = '\0'; snprintf(out, sz, "%s", tmp); }
}

static void fm_item_click_cb(lv_event_t *e)
{
    const app_fs_entry_t *ent = lv_event_get_user_data(e);
    if(ent->is_dir) {
        if(strcmp(ent->name, "..") == 0) {
            char par[96];
            fm_parent(par, sizeof(par));
            fm_refresh_path(par);
        } else {
            char full[128];
            fm_join(ent->name, full, sizeof(full));
            fm_refresh_path(full);
        }
    } else {
        char full[128];
        fm_join(ent->name, full, sizeof(full));
        fm_show_delete_dlg(full, false);
    }
}

static void fm_item_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    /* ESC: go up one level, or back to the main menu at root */
    if(strcmp(fm_path, "/") != 0) {
        char par[96];
        fm_parent(par, sizeof(par));
        fm_refresh_path(par);
    } else {
        back_to_menu();
    }
}

static void fm_rebuild(void)
{
    lv_obj_clean(fm_list);
    lv_obj_scroll_to_y(fm_list, 0, LV_ANIM_OFF);

    char pl[96];
    snprintf(pl, sizeof(pl), "%s", fm_path);
    lv_label_set_text(fm_path_lbl, pl);

    app_fs_entry_t *entries = NULL;
    int n = app_fs_list(fm_path, &entries);

    if(strcmp(fm_path, "/") != 0) {
        app_fs_entry_t up;
        snprintf(up.name, sizeof(up.name), "..");
        up.is_dir = true;
        up.size = 0;
        make_row(fm_list, g_fm, "..", "PARENT", &up, fm_item_click_cb, fm_item_key_cb);
    }

    if(n > 0) {
        for(int i = 0; i < n; i++) {
            char display[80];
            char size_s[16] = "";
            if(entries[i].is_dir) {
                snprintf(display, sizeof(display), "%s/", entries[i].name);
            } else {
                snprintf(display, sizeof(display), "%s", entries[i].name);
                if(entries[i].size >= 1024 * 1024)
                    snprintf(size_s, sizeof(size_s), "%.1fMB", entries[i].size / (1024.0 * 1024.0));
                else if(entries[i].size >= 1024)
                    snprintf(size_s, sizeof(size_s), "%.1fKB", entries[i].size / 1024.0);
                else
                    snprintf(size_s, sizeof(size_s), "%u B", entries[i].size);
            }
            make_row(fm_list, g_fm, display, entries[i].is_dir ? NULL : size_s,
                     &entries[i], fm_item_click_cb, fm_item_key_cb);
        }
        app_fs_free_entries(entries);
    } else {
        lv_obj_t *empty = lv_label_create(fm_list);
        lv_label_set_text(empty, "EMPTY FOLDER");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x777777), 0);
    }

    /* DELETE action row (deletes the currently focused entry) */
    app_fs_entry_t del;
    snprintf(del.name, sizeof(del.name), "");
    del.is_dir = false;
    del.size = 0;
    lv_obj_t *dbtn = make_row(fm_list, g_fm, "DELETE SELECTED", "", &del, fm_delete_cb, fm_item_key_cb);
    lv_obj_set_style_bg_color(dbtn, lv_color_hex(0x4a2626), 0);
    lv_obj_set_style_bg_color(dbtn, lv_color_hex(0x5f3434), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(lv_obj_get_child(dbtn, 0), lv_color_hex(0xff8a80), 0);

    fm_focus_first();
}

static void fm_refresh_path(const char *path)
{
    snprintf(fm_path, sizeof(fm_path), "%s", path);
    fm_rebuild();
}

static lv_obj_t *fm_first_row(void)
{
    return lv_obj_get_child(fm_list, 0);
}

static void fm_focus_first(void)
{
    lv_obj_t *first = fm_first_row();
    if(first && lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE))
        lv_group_focus_obj(first);
}

static void fm_show_delete_dlg(const char *path, bool is_dir)
{
    const char *msg = is_dir ? "DELETE FOLDER AND ALL" : "DELETE FILE?";
    const char *sub = is_dir ? "CONTENTS? (RECURSIVE)" : "";
    char full[160];
    snprintf(full, sizeof(full), "%s\n%s", msg, sub);
    show_dialog(is_dir ? DLG_DELETE_DIR : DLG_DELETE_FILE, full, path, "DELETE");
}

static void create_fm_screen(void)
{
    g_fm = lv_group_create();
    row_focus_hook = fm_row_focus_cb;
    scr_fm = lv_obj_create(NULL);
    lv_obj_set_size(scr_fm, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr_fm, lv_color_hex(0x141418), 0);
    lv_obj_set_style_bg_opa(scr_fm, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_fm, 0, 0);

    make_header(scr_fm, "FILE MANAGER");

    lv_obj_t *path_bar = lv_obj_create(scr_fm);
    lv_obj_remove_flag(path_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(path_bar, SCR_W, 26);
    lv_obj_set_pos(path_bar, 0, HEADER_H);
    lv_obj_set_style_bg_color(path_bar, lv_color_hex(0x1b1b20), 0);
    lv_obj_set_style_bg_opa(path_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(path_bar, 0, 0);
    lv_obj_set_style_pad_hor(path_bar, 8, 0);

    fm_path_lbl = lv_label_create(path_bar);
    lv_label_set_text(fm_path_lbl, "/");
    lv_obj_set_style_text_color(fm_path_lbl, lv_color_hex(0x4fc3f7), 0);
    lv_obj_set_style_text_font(fm_path_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(fm_path_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    fm_list = lv_obj_create(scr_fm);
    lv_obj_set_size(fm_list, SCR_W, SCR_H - HEADER_H - 26 - 34);
    lv_obj_set_pos(fm_list, 0, HEADER_H + 26);
    lv_obj_set_style_bg_opa(fm_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fm_list, 0, 0);
    lv_obj_set_style_pad_all(fm_list, 2, 0);
    lv_obj_set_style_pad_top(fm_list, 4, 0);
    lv_obj_set_layout(fm_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(fm_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(fm_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(fm_list, 4, 0);

    lv_obj_t *fm_footer = lv_obj_create(scr_fm);
    lv_obj_remove_flag(fm_footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(fm_footer, SCR_W, 34);
    lv_obj_set_pos(fm_footer, 0, SCR_H - 34);
    lv_obj_set_style_bg_opa(fm_footer, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fm_footer, lv_color_hex(0x141418), 0);
    lv_obj_set_style_border_width(fm_footer, 0, 0);
    lv_obj_set_style_pad_all(fm_footer, 0, 0);
    lv_obj_set_layout(fm_footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(fm_footer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(fm_footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(fm_footer, 0, 0);
    lv_obj_t *f1 = lv_label_create(fm_footer);
    lv_label_set_text(f1, "OK: OPEN / DELETE FILE");
    lv_obj_set_style_text_color(f1, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(f1, &lv_font_montserrat_12, 0);
    lv_obj_t *f2 = lv_label_create(fm_footer);
    lv_label_set_text(f2, "HOLD-OK: BACK / DELETE FOLDER");
    lv_obj_set_style_text_color(f2, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(f2, &lv_font_montserrat_12, 0);
}

/*------------------------ offline programmer ------------------*/

typedef enum {
    ALGO_LIST,
    ALGO_DETAIL,
    ALGO_CHIP,
    ALGO_PICK,
} algo_view_t;

static bool algo_have_default(void);
static void algo_enter(void);
static void algo_show_view(algo_view_t v);

typedef enum {
    FL_LIST,
    FL_RUNNING,
    FL_RESULT,
} flash_view_t;

static lv_obj_t *flash_body;
static lv_obj_t *fl_bar, *fl_pct, *fl_file;
static lv_timer_t *fl_poll_timer;
static flash_view_t fl_view = FL_LIST;
static char fl_bin_path[128];
static int fl_result;
static char fl_err[96];

static void flash_show_view(flash_view_t v)
{
    fl_view = v;
    lv_obj_clean(flash_body);
    /* partial rendering: make sure the whole body is redrawn so old view
     * pixels cannot linger (SPI is slow; a stale region would be visible) */
    lv_obj_invalidate(flash_body);
}

static void flash_list_esc_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    back_to_menu();
}

static void flash_list_cb(lv_event_t *e)
{
    const app_fs_entry_t *ent = lv_event_get_user_data(e);
    if(strcmp(ent->name, "__ALGO__") == 0) {
        algo_enter();
        return;
    }
    if(strcmp(ent->name, "__BUILTIN__") == 0) {
        /* builtin test firmware - no filesystem involved */
        if(!algo_have_default()) {
            show_dialog(DLG_INFO, "NO DEFAULT ALGO", "CONFIGURE IN ALGO CONFIG", "SETUP");
            return;
        }
        show_dialog(DLG_FLASH_CONFIRM, "FLASH BUILTIN TEST FW?", "__builtin", "FLASH");
        return;
    }
    if(ent->is_dir) {
        back_to_menu();
        return;
    }
    /* require a default algorithm configured */
    if(!algo_have_default()) {
        show_dialog(DLG_INFO, "NO DEFAULT ALGO", "CONFIGURE IN ALGO CONFIG", "SETUP");
        return;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "FLASH '%s' TO TARGET?", ent->name);
    char full[128];
    snprintf(full, sizeof(full), "/%s", ent->name);
    show_dialog(DLG_FLASH_CONFIRM, msg, full, "FLASH");
}

static void flash_start_view(const char *path);
static void flash_kick_cb(lv_timer_t *t);
static void flash_result_back_cb(lv_event_t *e);
static void flash_result_key_cb(lv_event_t *e);

static lv_obj_t *flash_first_row(void)
{
    return lv_obj_get_child(flash_body, 1);
}

static void flash_refresh_list(void)
{
    flash_show_view(FL_LIST);

    lv_obj_t *lst0 = lv_obj_get_child(flash_body, 0);
    if(lst0) lv_obj_scroll_to_y(lst0, 0, LV_ANIM_OFF);

    lv_obj_t *head = lv_obj_create(flash_body);
    lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_height(head, 26);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_hor(head, 8, 0);
    lv_obj_t *pl = lv_label_create(head);
    lv_label_set_text(pl, "AVAILABLE FIRMWARE (/)");
    lv_obj_set_style_text_color(pl, lv_color_hex(0x888888), 0);

    lv_obj_t *lst = lv_obj_create(flash_body);
    lv_obj_set_width(lst, lv_pct(100));
    lv_obj_set_height(lst, lv_pct(100));
    lv_obj_set_style_bg_opa(lst, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lst, 0, 0);
    lv_obj_set_style_pad_all(lst, 0, 0);
    lv_obj_set_style_pad_top(lst, 2, 0);
    lv_obj_set_layout(lst, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(lst, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(lst, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(lst, 4, 0);

    app_fs_entry_t up;
    snprintf(up.name, sizeof(up.name), "..");
    up.is_dir = true;
    up.size = 0;
    make_row(lst, g_flash, "..", "MAIN MENU", &up, flash_list_cb, flash_list_esc_cb);

    app_fs_entry_t algo_ent;
    snprintf(algo_ent.name, sizeof(algo_ent.name), "..");
    snprintf(algo_ent.name, sizeof(algo_ent.name), "__ALGO__");
    algo_ent.is_dir = false;
    algo_ent.size = 0;
    lv_obj_t *ar = make_row(lst, g_flash, "ALGO CONFIG", ">>", &algo_ent, flash_list_cb, flash_list_esc_cb);
    lv_obj_set_style_text_color(lv_obj_get_child(ar, 0), lv_color_hex(0xffc46b), 0);

    /* builtin test firmware entry (filesystem bypassed at this stage) */
    app_fs_entry_t builtin_ent;
    snprintf(builtin_ent.name, sizeof(builtin_ent.name), "__BUILTIN__");
    builtin_ent.is_dir = false;
    builtin_ent.size = 4096;
    lv_obj_t *br = make_row(lst, g_flash, "BUILTIN TEST FW", "4KB", &builtin_ent, flash_list_cb, flash_list_esc_cb);
    lv_obj_set_style_text_color(lv_obj_get_child(br, 0), lv_color_hex(0x6ec6ff), 0);

    app_fs_entry_t *entries = NULL;
    int n = app_fs_list("/", &entries);
    int shown = 0;
    if(n > 0) {
        for(int i = 0; i < n; i++) {
            if(entries[i].is_dir) continue;
            size_t len = strlen(entries[i].name);
            if(len < 5 || app_stricmp(entries[i].name + len - 4, ".bin") != 0) continue;
            char size_s[16];
            snprintf(size_s, sizeof(size_s), "%.1fKB", entries[i].size / 1024.0);
            make_row(lst, g_flash, entries[i].name, size_s, &entries[i], flash_list_cb, flash_list_esc_cb);
            shown++;
        }
        app_fs_free_entries(entries);
    }

    if(shown == 0) {
        lv_obj_t *empty = lv_label_create(lst);
        lv_label_set_text(empty, "NO FIRMWARE FILES");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x777777), 0);
    }

    lv_obj_t *first = lv_obj_get_child(lst, 0);
    if(first && lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE)) lv_group_focus_obj(first);
}

static void flash_poll_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t done, total;
    int st = app_flash_poll(&done, &total);
    if(st == APP_FLASH_BUSY) {
        if(total) {
            uint32_t pct = done * 100 / total;
            lv_bar_set_value(fl_bar, pct, LV_ANIM_OFF);
            char buf[16];
            snprintf(buf, sizeof(buf), "%u%%", pct);
            lv_label_set_text(fl_pct, buf);
        }
        return;
    }

    lv_timer_delete(fl_poll_timer);
    fl_poll_timer = NULL;

    fl_result = st;
    if(st == APP_FLASH_FAIL)
        snprintf(fl_err, sizeof(fl_err), "%s", app_flash_last_reason());
    flash_show_view(FL_RESULT);

    bool ok = (fl_result == APP_FLASH_OK);
    lv_obj_t *res = lv_label_create(flash_body);
    lv_label_set_text(res, ok ? "SUCCESS" : "FAILED");
    lv_obj_set_style_text_color(res, ok ? lv_color_hex(0x2fce6a)
                                        : lv_color_hex(0xff3b30), 0);
    lv_obj_set_style_text_font(res, &lv_font_montserrat_24, 0);

    if(!ok) {
        lv_obj_t *err = lv_label_create(flash_body);
        lv_label_set_text(err, fl_err);
        lv_obj_set_style_text_color(err, lv_color_hex(0xcc4444), 0);
        lv_obj_set_style_text_font(err, &lv_font_montserrat_14, 0);
    }

    lv_obj_t *tip = lv_label_create(flash_body);
    lv_label_set_text(tip, ok ? "TARGET PROGRAMMED OK" : "PROGRAMMING FAILED");
    lv_obj_set_style_text_color(tip, lv_color_hex(0x888888), 0);

    lv_obj_t *back = lv_button_create(flash_body);
    lv_obj_set_width(back, 160);
    lv_obj_set_height(back, 38);
    lv_obj_set_style_bg_color(back, ok ? lv_color_hex(0x1f5f2f) : lv_color_hex(0x5f2f2f), 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x34344a), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_outline_width(back, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(back, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, "OK - FIRMWARE LIST");
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, flash_result_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(back, flash_result_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(g_flash, back);
    lv_group_focus_obj(back);
}

static void flash_result_back_cb(lv_event_t *e)
{
    (void)e;
    flash_refresh_list();
}

static void flash_result_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    flash_refresh_list();
}

static void flash_abort_cb(lv_event_t *e)
{
    (void)e;
    app_flash_request_cancel();
}

static void flash_cancel_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    app_flash_request_cancel();
}

static void flash_start_view(const char *path)
{
    snprintf(fl_bin_path, sizeof(fl_bin_path), "%s", path);
    flash_show_view(FL_RUNNING);

    lv_obj_t *head = lv_obj_create(flash_body);
    lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(head, SCR_W - 16, 22);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    fl_file = lv_label_create(head);
    lv_label_set_text(fl_file, path);
    lv_obj_set_style_text_color(fl_file, lv_color_hex(0x4fc3f7), 0);
    lv_obj_set_style_text_font(fl_file, &lv_font_montserrat_14, 0);
    lv_obj_align(fl_file, LV_ALIGN_TOP_LEFT, 0, 0);

    /* algorithm info line */
    app_algo_t defs[APP_ALGO_MAX];
    int dn = app_algo_load_all(defs, APP_ALGO_MAX);
    const app_algo_t *use_algo = NULL;
    for(int i = 0; i < dn; i++)
        if(defs[i].is_default) { use_algo = &defs[i]; break; }
    char alabel[64];
    if(use_algo)
        snprintf(alabel, sizeof(alabel), "ALGO: %s  RAM@0x%08X", use_algo->name, use_algo->ram_addr);
    else
        snprintf(alabel, sizeof(alabel), "ALGO: NONE");
    lv_obj_t *arow = lv_obj_create(flash_body);
    lv_obj_remove_flag(arow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(arow, SCR_W - 16, 14);
    lv_obj_set_style_pad_all(arow, 0, 0);
    lv_obj_set_style_bg_opa(arow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arow, 0, 0);
    lv_obj_t *al2 = lv_label_create(arow);
    lv_label_set_text(al2, alabel);
    lv_obj_set_style_text_color(al2, lv_color_hex(0xffc46b), 0);
    lv_obj_set_style_text_font(al2, &lv_font_montserrat_12, 0);
    lv_obj_align(al2, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *bar_row = lv_obj_create(flash_body);
    lv_obj_remove_flag(bar_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bar_row, SCR_W - 16, 18);
    lv_obj_set_style_pad_all(bar_row, 0, 0);
    lv_obj_set_style_bg_opa(bar_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar_row, 0, 0);
    fl_bar = lv_bar_create(bar_row);
    lv_obj_set_size(fl_bar, lv_pct(100), 14);
    lv_obj_set_style_bg_color(fl_bar, lv_color_hex(0x2a2a32), 0);
    lv_obj_set_style_bg_color(fl_bar, lv_color_hex(0x2f8f4e), LV_PART_INDICATOR);
    lv_bar_set_range(fl_bar, 0, 100);
    lv_bar_set_value(fl_bar, 0, LV_ANIM_OFF);
    lv_obj_center(fl_bar);

    /* fixed-width box: percentage text width changes must not reflow
     * the flex column (that moved tip/ABORT around and caused flicker) */
    lv_obj_t *pct_row = lv_obj_create(flash_body);
    lv_obj_remove_flag(pct_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pct_row, 70, 26);
    lv_obj_set_style_pad_all(pct_row, 0, 0);
    lv_obj_set_style_bg_opa(pct_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pct_row, 0, 0);
    fl_pct = lv_label_create(pct_row);
    lv_label_set_text(fl_pct, "0%");
    lv_obj_set_width(fl_pct, lv_pct(100));
    lv_obj_set_style_text_align(fl_pct, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(fl_pct, lv_color_hex(0x2f8f4e), 0);
    lv_obj_set_style_text_font(fl_pct, &lv_font_montserrat_20, 0);

    lv_obj_t *tip = lv_label_create(flash_body);
    lv_label_set_text(tip, "PROGRAMMING FLASH ...");
    lv_obj_set_style_text_color(tip, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(tip, &lv_font_montserrat_12, 0);

    /* ABORT button: visible cancel target (OK click or ESC cancels) */
    lv_obj_t *abort = lv_button_create(flash_body);
    lv_obj_set_size(abort, 140, 36);
    lv_obj_set_style_bg_color(abort, lv_color_hex(0x5f2f2f), 0);
    lv_obj_set_style_bg_color(abort, lv_color_hex(0x8a4040), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(abort, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(abort, 0, 0);
    lv_obj_set_style_shadow_width(abort, 0, 0);
    lv_obj_set_style_outline_width(abort, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_opa(abort, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_obj_t *al = lv_label_create(abort);
    lv_label_set_text(al, "ABORT");
    lv_obj_center(al);
    lv_obj_add_event_cb(abort, flash_abort_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(abort, flash_cancel_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(g_flash, abort);
    lv_group_focus_obj(abort);

    /* Let LVGL render the running screen first, then kick the flashing
     * from a one-shot timer. Starting right after a synchronous refresh
     * disturbed the SWD line, and starting immediately starves the UI
     * render (the flash thread preempts LVGL). */
    lv_timer_t *kick = lv_timer_create(flash_kick_cb, 30, NULL);
    lv_timer_set_repeat_count(kick, 1);
}

static void flash_kick_cb(lv_timer_t *t)
{
    (void)t;
    app_flash_start(fl_bin_path);
    fl_poll_timer = lv_timer_create(flash_poll_cb, 100, NULL);
}

static void create_flash_screen(void)
{
    g_flash = lv_group_create();
    row_focus_hook = NULL;
    scr_flash = lv_obj_create(NULL);
    lv_obj_set_size(scr_flash, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr_flash, lv_color_hex(0x141418), 0);
    lv_obj_set_style_bg_opa(scr_flash, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_flash, 0, 0);

    make_header(scr_flash, "OFFLINE PROGRAMMER");
    flash_body = make_body(scr_flash);
    lv_obj_set_style_pad_row(flash_body, 6, 0);
    make_footer(scr_flash, "OK: SELECT  HOLD-OK: CANCEL");
}

/*-------------------------- algo config menu -------------------*/

static lv_obj_t *scr_algo;
static lv_group_t *g_algo;
static lv_obj_t *algo_body;
static algo_view_t algo_view = ALGO_LIST;
static app_algo_t algo_list[APP_ALGO_MAX];
static int algo_count = 0;
static int algo_sel = 0;

static int tmpl_sel = 0;

static const char *chip_templates[] = {
    "APM32F407", "APM32F103", "STM32F103C8", "STM32F407",
    "STM32F429", "STM32F411", "GD32F407", "GD32F103",
    NULL
};

static void algo_back_to_flash(void)
{
    flash_refresh_list();
    ui_enter_screen(scr_flash, g_flash, flash_first_row());
}

static void algo_list_esc_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    algo_back_to_flash();
}

static void algo_detail_esc_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    algo_show_view(ALGO_LIST);
}

static void algo_chip_esc_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    algo_show_view(ALGO_LIST);
}

static void algo_pick_esc_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    algo_show_view(ALGO_CHIP);
}

/* ---------- ALGO_LIST ---------- */

static void algo_list_cb(lv_event_t *e)
{
    const app_fs_entry_t *ent = lv_event_get_user_data(e);
    if(strcmp(ent->name, "__ADD__") == 0) {
        algo_show_view(ALGO_CHIP);
        return;
    }
    for(int i = 0; i < algo_count; i++) {
        if(strcmp(algo_list[i].name, ent->name) == 0) {
            algo_sel = i;
            algo_show_view(ALGO_DETAIL);
            return;
        }
    }
}

static void algo_build_list(void)
{
    lv_obj_clean(algo_body);
    algo_count = app_algo_load_all(algo_list, APP_ALGO_MAX);
    if(algo_count < 0) algo_count = 0;

    for(int i = 0; i < algo_count; i++) {
        app_fs_entry_t ent;
        snprintf(ent.name, sizeof(ent.name), "%s", algo_list[i].name);
        ent.is_dir = false;
        ent.size = 0;
        char tag[40];
        snprintf(tag, sizeof(tag), "%s%s", algo_list[i].chip,
                 algo_list[i].is_default ? " [D]" : "");
        lv_obj_t *r = make_row(algo_body, g_algo, algo_list[i].name, tag,
                               &ent, algo_list_cb, algo_list_esc_cb);
        if(algo_list[i].is_default)
            lv_obj_set_style_text_color(lv_obj_get_child(r, 1), lv_color_hex(0x2fce6a), 0);
    }

    app_fs_entry_t add;
    snprintf(add.name, sizeof(add.name), "__ADD__");
    add.is_dir = false;
    add.size = 0;
    lv_obj_t *ar = make_row(algo_body, g_algo, "ADD NEW ALGO", "", &add, algo_list_cb, algo_list_esc_cb);
    lv_obj_set_style_bg_color(ar, lv_color_hex(0x1f3a2a), 0);
    lv_obj_set_style_bg_color(ar, lv_color_hex(0x2a5a3a), LV_STATE_FOCUSED);

    lv_obj_t *first = lv_obj_get_child(algo_body, 0);
    if(first && lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE)) lv_group_focus_obj(first);
}

/* ---------- ALGO_DETAIL ---------- */

static void algo_detail_action_cb(lv_event_t *e)
{
    const char *act = lv_event_get_user_data(e);
    if(strcmp(act, "DEFAULT") == 0) {
        app_algo_set_default(algo_list[algo_sel].name);
        algo_show_view(ALGO_DETAIL);
    } else if(strcmp(act, "DELETE") == 0) {
        show_dialog(DLG_ALGO_DELETE, "DELETE ALGORITHM?", algo_list[algo_sel].name, "DELETE");
    } else if(strcmp(act, "BACK") == 0) {
        algo_show_view(ALGO_LIST);
    }
}

static void algo_detail_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) != LV_KEY_ESC) return;
    lv_event_stop_bubbling(e);
    algo_show_view(ALGO_LIST);
}

static lv_obj_t *algo_action_row(const char *label, const char *act,
                                 lv_color_t bg, lv_color_t bg_focus)
{
    app_fs_entry_t ent;
    snprintf(ent.name, sizeof(ent.name), "%s", act);
    ent.is_dir = false;
    ent.size = 0;
    lv_obj_t *r = make_row(algo_body, g_algo, label, NULL, &ent,
                           algo_detail_action_cb, algo_detail_key_cb);
    lv_obj_set_style_bg_color(r, bg, 0);
    lv_obj_set_style_bg_color(r, bg_focus, LV_STATE_FOCUSED);
    return r;
}

static void algo_build_detail(void)
{
    lv_obj_clean(algo_body);
    if(algo_sel >= algo_count) return;
    app_algo_t *a = &algo_list[algo_sel];

    char line[64];
    snprintf(line, sizeof(line), "NAME: %s", a->name);
    make_row(algo_body, g_algo, line, NULL, NULL, NULL, NULL);
    snprintf(line, sizeof(line), "CHIP: %s", a->chip);
    make_row(algo_body, g_algo, line, NULL, NULL, NULL, NULL);
    snprintf(line, sizeof(line), "FLASH: %u KB", a->flash_size_kb);
    make_row(algo_body, g_algo, line, NULL, NULL, NULL, NULL);
    snprintf(line, sizeof(line), "RAM: 0x%08X", a->ram_addr);
    make_row(algo_body, g_algo, line, NULL, NULL, NULL, NULL);
    snprintf(line, sizeof(line), "BIN: %s", a->algo_bin);
    make_row(algo_body, g_algo, line, NULL, NULL, NULL, NULL);
    snprintf(line, sizeof(line), "PAGE: %u  SECTOR: %u", a->page_size, a->sector_size);
    make_row(algo_body, g_algo, line, NULL, NULL, NULL, NULL);

    if(a->is_default)
        algo_action_row("IS DEFAULT", "BACK", lv_color_hex(0x1f3a2a), lv_color_hex(0x2a5a3a));
    else
        algo_action_row("SET DEFAULT", "DEFAULT", lv_color_hex(0x1f3a2a), lv_color_hex(0x2a5a3a));
    algo_action_row("DELETE", "DELETE", lv_color_hex(0x4a2626), lv_color_hex(0x5f3434));
    algo_action_row("BACK", "BACK", lv_color_hex(0x26262e), lv_color_hex(0x34344a));

    lv_obj_t *first = lv_obj_get_child(algo_body, 6);
    if(first && lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE)) lv_group_focus_obj(first);
}

/* ---------- ALGO_CHIP (templates) ---------- */

static void algo_chip_cb(lv_event_t *e)
{
    const app_fs_entry_t *ent = lv_event_get_user_data(e);
    for(int i = 0; chip_templates[i]; i++) {
        if(strcmp(chip_templates[i], ent->name) == 0) {
            tmpl_sel = i;
            algo_show_view(ALGO_PICK);
            return;
        }
    }
}

static void algo_build_chip(void)
{
    lv_obj_clean(algo_body);
    for(int i = 0; chip_templates[i]; i++) {
        app_fs_entry_t ent;
        snprintf(ent.name, sizeof(ent.name), "%s", chip_templates[i]);
        ent.is_dir = false;
        ent.size = 0;
        make_row(algo_body, g_algo, chip_templates[i], NULL, &ent,
                 algo_chip_cb, algo_chip_esc_cb);
    }
    lv_obj_t *first = lv_obj_get_child(algo_body, 0);
    if(first && lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE)) lv_group_focus_obj(first);
}

/* ---------- ALGO_PICK (select algo bin) ---------- */

static void algo_pick_cb(lv_event_t *e)
{
    const app_fs_entry_t *ent = lv_event_get_user_data(e);
    if(strcmp(ent->name, "__NONE__") == 0) return;

    /* base name: strip "_algo.bin" or ".bin" */
    char base[APP_ALGO_NAME_MAX];
    snprintf(base, sizeof(base), "%s", ent->name);
    size_t len = strlen(base);
    if(len > 9 && strcmp(base + len - 9, "_algo.bin") == 0) base[len - 9] = '\0';
    else if(len > 4 && strcmp(base + len - 4, ".bin") == 0) base[len - 4] = '\0';

    app_algo_t a;
    if(app_algo_load(base, &a) == 0) {
        /* reuse cfg produced by flm2bin.py */
    } else {
        /* create from template (params unknown until cfg exists) */
        memset(&a, 0, sizeof(a));
        snprintf(a.name, sizeof(a.name), "%s", base);
        snprintf(a.chip, sizeof(a.chip), "%s",
                 chip_templates[tmpl_sel] ? chip_templates[tmpl_sel] : "GENERIC");
        snprintf(a.algo_bin, sizeof(a.algo_bin), "%s", ent->name);
    }
    if(app_algo_save(&a) == 0) {
        algo_show_view(ALGO_LIST);
    } else {
        show_dialog(DLG_INFO, "SAVE FAILED", "/algo WRITE ERROR", "OK");
    }
}

static void algo_build_pick(void)
{
    lv_obj_clean(algo_body);
    app_fs_entry_t *entries = NULL;
    int n = app_fs_list("/algo", &entries);
    int shown = 0;
    if(n > 0) {
        for(int i = 0; i < n; i++) {
            if(entries[i].is_dir) continue;
            size_t len = strlen(entries[i].name);
            if(len < 5 || strcmp(entries[i].name + len - 4, ".bin") != 0) continue;
            app_fs_entry_t ent;
            snprintf(ent.name, sizeof(ent.name), "%s", entries[i].name);
            ent.is_dir = false;
            ent.size = 0;
            make_row(algo_body, g_algo, entries[i].name, NULL, &ent,
                     algo_pick_cb, algo_pick_esc_cb);
            shown++;
        }
        app_fs_free_entries(entries);
    }
    if(shown == 0) {
        app_fs_entry_t none;
        snprintf(none.name, sizeof(none.name), "__NONE__");
        none.is_dir = false;
        none.size = 0;
        make_row(algo_body, g_algo, "NO ALGO BIN IN /algo", NULL, &none,
                 algo_pick_cb, algo_pick_esc_cb);
    }
    lv_obj_t *first = lv_obj_get_child(algo_body, 0);
    if(first && lv_obj_has_flag(first, LV_OBJ_FLAG_CLICKABLE)) lv_group_focus_obj(first);
}

/* ---------- view switching ---------- */

static void algo_show_view(algo_view_t v)
{
    algo_view = v;
    lv_obj_clean(algo_body);
    lv_obj_add_flag(algo_body, LV_OBJ_FLAG_HIDDEN);
    switch(v) {
    case ALGO_LIST:   algo_build_list();   break;
    case ALGO_DETAIL: algo_build_detail(); break;
    case ALGO_CHIP:   algo_build_chip();   break;
    case ALGO_PICK:   algo_build_pick();   break;
    }
    lv_obj_remove_flag(algo_body, LV_OBJ_FLAG_HIDDEN);
}

static void algo_enter(void)
{
    algo_show_view(ALGO_LIST);
    ui_enter_screen(scr_algo, g_algo, lv_obj_get_child(algo_body, 0));
}

static bool algo_have_default(void)
{
    app_algo_t tmp[APP_ALGO_MAX];
    int n = app_algo_load_all(tmp, APP_ALGO_MAX);
    for(int i = 0; i < n; i++)
        if(tmp[i].is_default) return true;
    return false;
}

static void create_algo_screen(void)
{
    g_algo = lv_group_create();
    row_focus_hook = NULL;
    scr_algo = lv_obj_create(NULL);
    lv_obj_set_size(scr_algo, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(scr_algo, lv_color_hex(0x141418), 0);
    lv_obj_set_style_bg_opa(scr_algo, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr_algo, 0, 0);

    make_header(scr_algo, "ALGO CONFIG");
    algo_body = make_body(scr_algo);
    lv_obj_set_style_pad_row(algo_body, 4, 0);
    make_footer(scr_algo, "OK SELECT  HOLD-OK: BACK");
}

/*-------------------------- dialog actions --------------------*/

static void dlg_yes_cb(lv_event_t *e)
{
    (void)e;
    dlg_kind_t k = dlg.kind;
    char target[96];
    snprintf(target, sizeof(target), "%s", dlg.target);
    dialog_cleanup();

    if(k == DLG_DELETE_FILE || k == DLG_DELETE_DIR) {
        app_fs_delete(target);
        fm_rebuild();
    } else if(k == DLG_FLASH_CONFIRM) {
        flash_start_view(target);
    } else if(k == DLG_INFO) {
        if(strcmp(dlg.yes_label, "SETUP") == 0) algo_enter();
    } else if(k == DLG_ALGO_DELETE) {
        app_algo_delete(target);
        algo_show_view(ALGO_LIST);
    }
}

static void dlg_no_cb(lv_event_t *e)
{
    (void)e;
    dialog_cleanup();
}

/*------------------------------ entry -------------------------*/

void app_ui_init(void)
{
    lv_theme_default_init(lv_display_get_default(),
                          lv_palette_main(LV_PALETTE_BLUE),
                          lv_palette_main(LV_PALETTE_BLUE),
                          true, &lv_font_montserrat_14);

    create_boot_screen();
    create_menu_screen();
    create_dap_screen();
    create_fm_screen();
    create_flash_screen();
    create_algo_screen();

    lv_screen_load(scr_boot);
}

void app_ui_enter_menu(void)
{
    ui_enter_screen(scr_menu, g_menu, lv_obj_get_child(scr_menu, 1));
    scr_depth = 0;
    push_screen(SCR_MENU);
}
