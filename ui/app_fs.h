#ifndef APP_FS_H
#define APP_FS_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Filesystem abstraction for the file manager.
 *
 * Simulator (PC): host directory behind "disk/"  -> sim/sim_fs.c
 * MCU:            FileX FAT on W25Q128            -> User/ (later)
 */

typedef struct {
    char     name[64];
    bool     is_dir;
    uint32_t size;          /* bytes, 0 for folders */
} app_fs_entry_t;

/* List directory contents (sorted: folders first, then files, ".." not included).
 * Returns number of entries, or -1 on error. `*entries` must be freed with
 * app_fs_free_entries(). Paths are absolute from volume root, e.g. "/", "/firmware". */
int app_fs_list(const char *path, app_fs_entry_t **entries);

void app_fs_free_entries(app_fs_entry_t *entries);

/* Recursively delete a folder, or delete a file. 0 on success, -1 on error. */
int app_fs_delete(const char *path);

/* True if the path exists and is a folder. */
bool app_fs_is_dir(const char *path);

/* Read a whole file into buf (max `size` bytes, NUL terminated).
 * Returns bytes read (>=0), or -1 on error. */
int app_fs_read(const char *path, char *buf, uint32_t size);

/* Create/overwrite a file with the given content. 0 on success, -1 on error. */
int app_fs_write(const char *path, const char *buf, uint32_t size);

#endif
