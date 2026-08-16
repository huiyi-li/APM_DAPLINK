/*
 * Simulator filesystem: maps the UI volume ("/") to a host directory.
 * Usage: app_fs_sim_set_root("/path/to/disk")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "app_fs.h"

extern const char *sim_disk_root;

static void build_host_path(const char *vpath, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s%s", sim_disk_root, vpath);
    if(out[0] && out[strlen(out) - 1] == '/')
        out[strlen(out) - 1] = '\0';
}

static int name_cmp(const void *a, const void *b)
{
    const app_fs_entry_t *ea = a, *eb = b;
    if(ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    return strcasecmp(ea->name, eb->name);
}

int app_fs_list(const char *path, app_fs_entry_t **entries)
{
    char hp[512];
    build_host_path(path, hp, sizeof(hp));

    DIR *d = opendir(hp);
    if(!d) return -1;

    app_fs_entry_t *arr = NULL;
    int count = 0;

    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;

        char full[640];
        snprintf(full, sizeof(full), "%s/%s", hp, de->d_name);

        struct stat st;
        if(stat(full, &st) != 0) continue;

        app_fs_entry_t e;
        memset(&e, 0, sizeof(e));
        snprintf(e.name, sizeof(e.name), "%s", de->d_name);
        e.is_dir = S_ISDIR(st.st_mode);
        e.size = e.is_dir ? 0 : (uint32_t)st.st_size;

        arr = realloc(arr, (count + 1) * sizeof(app_fs_entry_t));
        arr[count++] = e;
    }
    closedir(d);

    if(arr) qsort(arr, count, sizeof(app_fs_entry_t), name_cmp);

    *entries = arr;
    return count;
}

void app_fs_free_entries(app_fs_entry_t *entries)
{
    free(entries);
}

static int delete_recursive(const char *path)
{
    DIR *d = opendir(path);
    if(!d) return -1;

    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char full[640];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
        struct stat st;
        if(stat(full, &st) != 0) continue;
        if(S_ISDIR(st.st_mode)) {
            delete_recursive(full);
            rmdir(full);
        } else {
            remove(full);
        }
    }
    closedir(d);
    return 0;
}

int app_fs_delete(const char *path)
{
    char hp[512];
    build_host_path(path, hp, sizeof(hp));
    struct stat st;
    if(stat(hp, &st) != 0) return -1;
    if(S_ISDIR(st.st_mode)) {
        delete_recursive(hp);
        return rmdir(hp);
    }
    return remove(hp);
}

bool app_fs_is_dir(const char *path)
{
    char hp[512];
    build_host_path(path, hp, sizeof(hp));
    struct stat st;
    return (stat(hp, &st) == 0) && S_ISDIR(st.st_mode);
}

int app_fs_read(const char *path, char *buf, uint32_t size)
{
    char hp[512];
    build_host_path(path, hp, sizeof(hp));
    FILE *f = fopen(hp, "rb");
    if(!f) return -1;
    size_t n = fread(buf, 1, size - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (int)n;
}

int app_fs_write(const char *path, const char *buf, uint32_t size)
{
    char hp[512];
    build_host_path(path, hp, sizeof(hp));
    FILE *f = fopen(hp, "wb");
    if(!f) return -1;
    size_t n = fwrite(buf, 1, size, f);
    fclose(f);
    return (n == size) ? 0 : -1;
}
