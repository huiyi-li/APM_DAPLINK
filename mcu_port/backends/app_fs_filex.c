/*
 * MCU backend: FileX FAT filesystem on the W25Q128 volume.
 *
 * Implements the ui/app_fs.h API used by the file manager and the
 * algorithm config screens. The volume is the FileX FAT volume that
 * filex_demo.c opens (shared via the demo media handle).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "app_fs.h"
#include "fx_api.h"

/* media handle owned by filex_demo.c */
extern FX_MEDIA *filex_demo_get_media(void);

/* FileX paths are relative to the opened media (no volume prefix) */

/* virtual path like "/firmware" -> "A:\\firmware" */
static void build_fx_path(const char *vpath, char *out, size_t outsz)
{
    const char *p = (vpath[0] == '/') ? vpath + 1 : vpath;
    size_t i;

    if (p[0] == '\0')
    {
        /* root: FileX uses a single backslash for the root directory */
        snprintf(out, outsz, "\\");
        return;
    }
    for (i = 0; p[i] != '\0' && i + 1 < outsz; i++)
    {
        out[i] = (p[i] == '/') ? '\\' : p[i];
    }
    out[i] = '\0';
}

static int app_str_icmp(const char *a, const char *b)
{
    int ca, cb;

    do
    {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
    }
    while (ca != 0 && ca == cb);
    return ca - cb;
}

static int name_cmp(const void *a, const void *b)
{
    const app_fs_entry_t *ea = (const app_fs_entry_t *)a;
    const app_fs_entry_t *eb = (const app_fs_entry_t *)b;
    int d;

    if (ea->is_dir != eb->is_dir)
    {
        return ea->is_dir ? -1 : 1;
    }
    d = app_str_icmp(ea->name, eb->name);
    return (d < 0) ? -1 : (d > 0 ? 1 : 0);
}

static char *fx_get_name(const char *fxpath)
{
    const char *p = strrchr(fxpath, '\\');
    return (char *)(p ? p + 1 : fxpath);
}

int app_fs_list(const char *path, app_fs_entry_t **entries)
{
    FX_MEDIA *media = filex_demo_get_media();
    static app_fs_entry_t pool[64];
    char fxpath[128];
    char name[256];
    UINT attr;
    ULONG size;
    UINT y, mo, d, h, mi, s;
    UINT status;
    int n = 0;

    if (media == NULL || entries == NULL)
    {
        return -1;
    }
    build_fx_path(path, fxpath, sizeof(fxpath));

    status = fx_directory_first_full_entry_find(media, fxpath, &attr, &size,
                                                &y, &mo, &d, &h, &mi, &s);
    while (status == FX_SUCCESS && n < 64)
    {
        (void)fx_directory_short_name_get(media, media->fx_media_name_buffer,
                                          name);
        snprintf(pool[n].name, sizeof(pool[n].name), "%s", name);
        pool[n].is_dir = (attr & FX_DIRECTORY) != 0U;
        pool[n].size = pool[n].is_dir ? 0U : size;
        n++;
        status = fx_directory_next_full_entry_find(media, fxpath, &attr, &size,
                                                   &y, &mo, &d, &h, &mi, &s);
    }

    qsort(pool, (size_t)n, sizeof(app_fs_entry_t), name_cmp);
    *entries = pool;
    return n;
}

void app_fs_free_entries(app_fs_entry_t *entries)
{
    (void)entries;   /* static pool, nothing to free */
}

int app_fs_delete(const char *path)
{
    FX_MEDIA *media = filex_demo_get_media();
    char fxpath[128];

    if (media == NULL) return -1;
    build_fx_path(path, fxpath, sizeof(fxpath));

    if (app_fs_is_dir(path))
    {
        return fx_directory_delete(media, fxpath) == FX_SUCCESS ? 0 : -1;
    }
    return fx_file_delete(media, fxpath) == FX_SUCCESS ? 0 : -1;
}

bool app_fs_is_dir(const char *path)
{
    FX_MEDIA *media = filex_demo_get_media();
    char fxpath[128];
    UINT attr;
    ULONG size;
    UINT y, mo, d, h, mi, s;

    if (media == NULL) return false;
    build_fx_path(path, fxpath, sizeof(fxpath));

    if (fx_directory_information_get(media, fxpath, &attr, &size,
                                     &y, &mo, &d, &h, &mi, &s) != FX_SUCCESS)
    {
        return false;
    }
    return (attr & FX_DIRECTORY) != 0U;
}

int app_fs_read(const char *path, char *buf, uint32_t size)
{
    FX_MEDIA *media = filex_demo_get_media();
    FX_FILE file;
    char fxpath[128];
    ULONG actual = 0;
    UINT status;

    if (media == NULL || buf == NULL || size == 0U) return -1;
    build_fx_path(path, fxpath, sizeof(fxpath));

    status = fx_file_open(media, &file, fxpath, FX_OPEN_FOR_READ);
    if (status != FX_SUCCESS) return -1;

    status = fx_file_read(&file, buf, size - 1U, &actual);
    (void)fx_file_close(&file);
    if (status != FX_SUCCESS) return -1;

    buf[actual] = '\0';
    return (int)actual;
}

int app_fs_write(const char *path, const char *buf, uint32_t size)
{
    FX_MEDIA *media = filex_demo_get_media();
    FX_FILE file;
    char fxpath[128];
    UINT status;

    if (media == NULL || buf == NULL) return -1;
    build_fx_path(path, fxpath, sizeof(fxpath));

    (void)fx_file_delete(media, fxpath);
    status = fx_file_create(media, fxpath);
    if (status != FX_SUCCESS) return -1;

    status = fx_file_open(media, &file, fxpath, FX_OPEN_FOR_WRITE);
    if (status != FX_SUCCESS) return -1;

    status = fx_file_write(&file, (void *)buf, size);
    (void)fx_file_close(&file);
    return (status == FX_SUCCESS) ? 0 : -1;
}
