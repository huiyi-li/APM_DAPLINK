#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdbool.h>
#include <stdint.h>

/* Initializes the LVGL library and binds it to the ST7789 display via
 * display_port. Must be called once, from a thread, after the display
 * driver is ready. */
bool lvgl_port_init(void);

/* Runs the LVGL event loop (timers, animations, redraw). Call
 * periodically from a thread (e.g. every 5ms). */
void lvgl_port_handler(void);

#endif
