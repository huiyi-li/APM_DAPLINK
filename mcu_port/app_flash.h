#ifndef APP_FLASH_H
#define APP_FLASH_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Offline programming abstraction (poll based, non-blocking).
 *
 * Simulator (PC): fake engine in a thread      -> sim/sim_flash.c
 * MCU:            DAP SWD flash in a task      -> User/ (later)
 * UI drives everything with an lv_timer polling app_flash_poll().
 */

#define APP_FLASH_BUSY   0
#define APP_FLASH_OK     1
#define APP_FLASH_FAIL  -1

/* Kick off programming of the given .bin. Returns 0 on success. */
int app_flash_start(const char *path);

/* Poll progress. Returns APP_FLASH_*; done/total valid while busy. */
int app_flash_poll(uint32_t *done, uint32_t *total);

/* Request cancellation (checked by the engine). */
void app_flash_request_cancel(void);

/* Human readable failure reason (valid after APP_FLASH_FAIL). */
const char *app_flash_last_reason(void);

/* SWD clock for offline flashing, in Hz. 0 selects the fast clock
 * (no delay loop). Persisted in /settings.txt as flash_clock=... */
uint32_t app_flash_get_clock(void);
int app_flash_set_clock(uint32_t hz);

#endif
