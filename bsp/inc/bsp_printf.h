#ifndef BSP_PRINTF_H
#define BSP_PRINTF_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void bsp_debug_uart_init(uint32_t baud_rate);
size_t bsp_debug_uart_write(const uint8_t *data, size_t length);
void bsp_printf_lock(void);
void bsp_printf_unlock(void);

/*
 * Serialize every printf call. newlib stdio is not thread-safe: two
 * threads printf'ing at the same time corrupt the shared stdout FILE
 * structure (buffer pointers), which later makes __sflush_r call a
 * garbage _write pointer -> hard fault (SFERR). fprintf is used under
 * the hood so the macro does not recurse on itself.
 */
#define printf(...) do { bsp_printf_lock(); (void)fprintf(stdout, __VA_ARGS__); bsp_printf_unlock(); } while (0)

#endif
