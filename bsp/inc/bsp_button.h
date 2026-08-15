#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#include "button.h"

/* Board buttons on PE3/PE4/PE5, active low. */
typedef enum
{
    BSP_BUTTON_0 = 0,   /* PE3 */
    BSP_BUTTON_1,       /* PE4 */
    BSP_BUTTON_2,       /* PE5 */
    BSP_BUTTON_COUNT
} BSP_BUTTON_T;

void bsp_button_init(void);
void bsp_button_process(uint32_t now_ms);

/* Application level event subscription. The callback is invoked after
 * the built-in debug log, in the context of bsp_button_process(). */
typedef void (*BSP_BUTTON_EVENT_CB)(BSP_BUTTON_T button,
                                    BUTTON_EVENT_T event,
                                    uint32_t param,
                                    uint32_t now_ms,
                                    void *user_data);
void bsp_button_set_event_cb(BSP_BUTTON_EVENT_CB cb, void *user_data);

/* Convenience wrappers around the button framework. */
bool bsp_button_is_pressed(BSP_BUTTON_T button);
uint32_t bsp_button_press_duration_ms(BSP_BUTTON_T button, uint32_t now_ms);

/* Returns the underlying framework instance for advanced use. */
BUTTON_T *bsp_button_get(BSP_BUTTON_T button);

#endif
