#ifndef __USER_USB_INIT_H__
#define __USER_USB_INIT_H__

#include "usb_config.h"
#include <stdint.h>
typedef enum
{
    USBD_USER_RESET = 1,
    USBD_USER_RESUME,
    USBD_USER_SUSPEND,
    USBD_USER_CONNECT,
    USBD_USER_DISCONNECT,
    USBD_USER_ENUM_DONE,
    USBD_USER_ERROR,
} USBH_USER_STATUS;

void usb_event_handler(uint8_t busid, uint8_t event);

#endif /* __USER_USB_INIT_H__ */