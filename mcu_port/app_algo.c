/*
 * Flash algorithm configuration storage (text key=value cfg in /algo/).
 * Shared between the PC simulator and the MCU (FileX backend later).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_fs.h"
#include "app_algo.h"

#define ALGO_DIR "/"

/*----------------- tiny key=value parser/generator -----------------*/

typedef struct {
    char key[24];
    char val[96];
} kv_t;

static const char *trim(const char *s)
{
    while(*s == ' ' || *s == '\t') s++;
    return s;
}

/* parse cfg content into kv array, returns count */
static int cfg_parse(const char *text, kv_t *kvs, int max)
{
    int n = 0;
    const char *p = text;
    while(*p && n < max) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char line[160];
        if(len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        p += len;
        if(*p == '\n') p++;

        char *c = line;
        while(*c == ' ' || *c == '\t') c++;
        if(*c == '#' || *c == ';' || *c == '\0') continue;

        char *eq = strchr(c, '=');
        if(!eq) continue;
        *eq = '\0';
        snprintf(kvs[n].key, sizeof(kvs[n].key), "%s", trim(c));
        snprintf(kvs[n].val, sizeof(kvs[n].val), "%s", trim(eq + 1));
        n++;
    }
    return n;
}

static const char *cfg_get(const kv_t *kvs, int n, const char *key)
{
    for(int i = 0; i < n; i++)
        if(strcmp(kvs[i].key, key) == 0) return kvs[i].val;
    return NULL;
}

static uint32_t cfg_get_u32(const kv_t *kvs, int n, const char *key, uint32_t def)
{
    const char *v = cfg_get(kvs, n, key);
    if(!v || !*v) return def;
    return (uint32_t)strtoul(v, NULL, 0);
}

static int cfg_get_bool(const kv_t *kvs, int n, const char *key)
{
    const char *v = cfg_get(kvs, n, key);
    if(!v) return 0;
    return (strcmp(v, "1") == 0) || (strcmp(v, "true") == 0);
}

static int cfg_path(char *out, size_t sz, const char *name)
{
    return snprintf(out, sz, ALGO_DIR "/%s.cfg", name) > 0 ? 0 : -1;
}

/* Builtin default algorithm (GD32F470). The W25Q128 filesystem may be
 * unavailable during bring-up, so the default used by the offline
 * programmer is hardcoded here (mirrors flm2bin.py output). */
void app_algo_builtin_default(app_algo_t *a)
{
    static const struct { uint32_t size; uint32_t base; } sec[] = {
        { 0x4000U, 0x0U }, { 0x4000U, 0x4000U }, { 0x4000U, 0x8000U },
        { 0x4000U, 0xC000U }, { 0x10000U, 0x10000U },
        { 0x20000U, 0x20000U }, { 0x20000U, 0x40000U },
        { 0x20000U, 0x60000U }, { 0x20000U, 0x80000U },
        { 0x20000U, 0xA0000U }, { 0x20000U, 0xC0000U },
        { 0x20000U, 0xE0000U },
    };

    memset(a, 0, sizeof(*a));
    snprintf(a->name, sizeof(a->name), "GD32F470");
    snprintf(a->chip, sizeof(a->chip), "GD32F470");
    a->flash_size_kb = 1024;
    a->flash_base    = 0x08000000U;
    a->ram_addr      = 0x20000000U;
    snprintf(a->algo_bin, sizeof(a->algo_bin), "GD32F470_algo.bin");
    a->algo_size     = 788U;
    a->is_default    = true;
    a->fn_init       = 0x78U;
    a->fn_uninit     = 0x10CU;
    a->fn_erase_chip = 0x12CU;
    a->fn_erase_sector = 0x18CU;
    a->fn_program_page = 0x23CU;
    a->page_size     = 0x400U;
    a->sector_size   = 0x4000U;
    for (size_t i = 0; i < sizeof(sec) / sizeof(sec[0]); i++)
    {
        a->sectors[i].size = sec[i].size;
        a->sectors[i].base = sec[i].base;
    }
    a->sectors[12].size = 0;
}

/*----------------------------- API --------------------------------*/

static void algo_defaults(app_algo_t *a)
{
    memset(a, 0, sizeof(*a));
}

int app_algo_load(const char *name, app_algo_t *a)
{
    char path[128];
    if(cfg_path(path, sizeof(path), name) != 0) return -1;

    char buf[1024];
    int n = app_fs_read(path, buf, sizeof(buf));
    if(n < 0) return -1;

    kv_t kvs[24];
    int kn = cfg_parse(buf, kvs, 24);

    algo_defaults(a);
    const char *v;

    v = cfg_get(kvs, kn, "name");
    if(v) snprintf(a->name, sizeof(a->name), "%s", v);
    else snprintf(a->name, sizeof(a->name), "%s", name);

    v = cfg_get(kvs, kn, "chip");
    if(v) snprintf(a->chip, sizeof(a->chip), "%s", v);
    else snprintf(a->chip, sizeof(a->chip), "-");

    a->flash_size_kb  = cfg_get_u32(kvs, kn, "flash_size_kb", 0);
    a->flash_base     = cfg_get_u32(kvs, kn, "flash_base", 0);
    a->ram_addr       = cfg_get_u32(kvs, kn, "ram_addr", 0);
    a->algo_size      = cfg_get_u32(kvs, kn, "algo_size", 0);
    v = cfg_get(kvs, kn, "algo_bin");
    if(v) snprintf(a->algo_bin, sizeof(a->algo_bin), "%s", v);
    else snprintf(a->algo_bin, sizeof(a->algo_bin), "-");
    a->is_default     = cfg_get_bool(kvs, kn, "default");
    a->fn_init        = cfg_get_u32(kvs, kn, "Init", 0);
    a->fn_uninit      = cfg_get_u32(kvs, kn, "UnInit", 0);
    a->fn_erase_chip  = cfg_get_u32(kvs, kn, "EraseChip", 0);
    a->fn_erase_sector= cfg_get_u32(kvs, kn, "EraseSector", 0);
    a->fn_program_page= cfg_get_u32(kvs, kn, "ProgramPage", 0);
    a->page_size      = cfg_get_u32(kvs, kn, "page_size", 0);
    a->sector_size    = cfg_get_u32(kvs, kn, "sector_size", 0);

    if (a->fn_program_page == 0 && strcmp(name, "GD32F470") == 0)
    {
        /* filesystem entry missing/partial: use the builtin algorithm */
        app_algo_builtin_default(a);
        return 0;
    }

    /* sectors=size@base,size@base,... */
    {
        int si = 0;
        const char *sv = cfg_get(kvs, kn, "sectors");
        if(sv) {
            char tmp[192];
            snprintf(tmp, sizeof(tmp), "%s", sv);
            char *tok = strtok(tmp, ",");
            while(tok && si < APP_ALGO_SECTORS_MAX) {
                char *at = strchr(tok, '@');
                if(at) {
                    *at = '\0';
                    a->sectors[si].size = (uint32_t)strtoul(tok, NULL, 0);
                    a->sectors[si].base = (uint32_t)strtoul(at + 1, NULL, 0);
                    si++;
                }
                tok = strtok(NULL, ",");
            }
        }
        a->sectors[si].size = 0;
    }
    return 0;
}

int app_algo_save(const app_algo_t *a)
{
    char path[128];
    if(cfg_path(path, sizeof(path), a->name) != 0) return -1;

    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
        "# flash algorithm config (generated by flm2bin.py / config menu)\n"
        "name=%s\n"
        "chip=%s\n"
        "flash_size_kb=%u\n"
        "flash_base=0x%08X\n"
        "ram_addr=0x%08X\n"
        "algo_bin=%s\n"
        "algo_size=%u\n"
        "default=%d\n"
        "Init=0x%X\n"
        "UnInit=0x%X\n"
        "EraseChip=0x%X\n"
        "EraseSector=0x%X\n"
        "ProgramPage=0x%X\n"
        "page_size=%u\n"
        "sector_size=%u\n",
        a->name, a->chip, a->flash_size_kb, a->flash_base, a->ram_addr,
        a->algo_bin, a->algo_size, a->is_default ? 1 : 0,
        a->fn_init, a->fn_uninit, a->fn_erase_chip, a->fn_erase_sector,
        a->fn_program_page, a->page_size, a->sector_size);
    if(n < 0 || n >= (int)sizeof(buf)) return -1;
    return app_fs_write(path, buf, (uint32_t)n);
}

int app_algo_delete(const char *name)
{
    char path[128];
    if(cfg_path(path, sizeof(path), name) != 0) return -1;
    return app_fs_delete(path);
}

int app_algo_set_default(const char *name)
{
    app_algo_t list[APP_ALGO_MAX];
    int n = app_algo_load_all(list, APP_ALGO_MAX);
    if(n < 0) return -1;
    for(int i = 0; i < n; i++) {
        bool def = (strcmp(list[i].name, name) == 0);
        if(list[i].is_default != def) {
            list[i].is_default = def;
            app_algo_save(&list[i]);
        }
    }
    return 0;
}

int app_algo_load_all(app_algo_t *list, int max)
{
    app_fs_entry_t *entries = NULL;
    int count = 0;
    bool have_default = false;

    int n = app_fs_list(ALGO_DIR, &entries);
    if(n > 0) {
        for(int i = 0; i < n && count < max; i++) {
            const char *nm = entries[i].name;
            size_t len = strlen(nm);
            if(entries[i].is_dir) continue;
            if(len < 5 || strcmp(nm + len - 4, ".cfg") != 0) continue;

            char base[APP_ALGO_NAME_MAX];
            snprintf(base, sizeof(base), "%.*s", (int)(len - 4), nm);
            if(app_algo_load(base, &list[count]) == 0) {
                if(list[count].is_default) have_default = true;
                count++;
            }
        }
        app_fs_free_entries(entries);
    }

    /* ensure a usable default algorithm is always present */
    if(!have_default && count < max) {
        app_algo_builtin_default(&list[count]);
        count++;
    }
    return count;
}
