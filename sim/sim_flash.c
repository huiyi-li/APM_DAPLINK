/*
 * Simulator flash engine: fake programming with realistic timing.
 * Runs in its own thread; UI polls app_flash_poll().
 * Bins whose name starts with "fail" fail at 62% to demo the error path.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "app_flash.h"

static pthread_t th;
static volatile int state;              /* APP_FLASH_BUSY/OK/FAIL */
static volatile uint32_t done, total;
static volatile int cancel_req;
static char fail_reason[96];

static char sim_flash_path[512];

static void *flash_thread(void *arg)
{
    const char *path = arg;

    /* virtual path "/app.bin" -> host path "sim/disk/app.bin" */
    extern const char *sim_disk_root;
    char hp[512];
    snprintf(hp, sizeof(hp), "%s%s", sim_disk_root, path);

    struct stat st;
    if(stat(hp, &st) != 0) {
        snprintf(fail_reason, sizeof(fail_reason), "file not found");
        state = APP_FLASH_FAIL;
        return NULL;
    }

    total = (uint32_t)st.st_size;
    done = 0;
    state = APP_FLASH_BUSY;
    cancel_req = 0;

    uint32_t steps = 60;
    for(uint32_t i = 1; i <= steps; i++) {
        for(int j = 0; j < 40 && !cancel_req; j++) usleep(2000);
        if(cancel_req) {
            snprintf(fail_reason, sizeof(fail_reason), "cancelled by user");
            state = APP_FLASH_FAIL;
            return NULL;
        }
        done = total * i / steps;
    }

    /* simulated verify: fail bins fail after "programming" */
    const char *base = strrchr(hp, '/');
    base = base ? base + 1 : hp;
    if(strncasecmp(base, "fail", 4) == 0) {
        snprintf(fail_reason, sizeof(fail_reason), "verify mismatch @ 0x%08X", (unsigned)(total * 62 / 100));
        state = APP_FLASH_FAIL;
    } else {
        state = APP_FLASH_OK;
    }
    return NULL;
}

int app_flash_start(const char *path)
{
    snprintf(sim_flash_path, sizeof(sim_flash_path), "%s", path);
    char *dup = strdup(path);
    state = APP_FLASH_BUSY;
    done = 0;
    total = 1;
    if(pthread_create(&th, NULL, flash_thread, dup) != 0) {
        free(dup);
        return -1;
    }
    return 0;
}

int app_flash_poll(uint32_t *d, uint32_t *t)
{
    *d = done;
    *t = total;
    return state;
}

void app_flash_request_cancel(void)
{
    cancel_req = 1;
}

const char *app_flash_last_reason(void)
{
    return fail_reason;
}

bool app_flash_cancel_requested(void)
{
    return cancel_req;
}


