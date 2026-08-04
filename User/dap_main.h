#ifndef DAP_MAIN_H
#define DAP_MAIN_H

#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_hid.h"
#include "usbd_msc.h"
#include "chry_ringbuffer.h"
#include "DAP_config.h"
#include "DAP.h"

#define DAP_IN_EP  0x81
#define DAP_OUT_EP 0x01

#define CDC_IN_EP  0x82
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define HID_IN_EP  0x84
#define HID_OUT_EP 0x04

#define MSC_IN_EP  0x86
#define MSC_OUT_EP 0x07

#define CONFIG_DAP_HID 1


#define USBD_VID           0x0D28
#define USBD_PID           0x0204
#define USBD_MAX_POWER     500
#define USBD_LANGID_STRING 1033

#define CMSIS_DAP_INTERFACE_SIZE (9 + 7 + 7)

#ifdef CONFIG_DAP_HID
#define HID_PACKET_SIZE             64
#define CONFIG_HID_DESCRIPTOR_LEN   (9 + 9 + 7 + 7)
#define CONFIG_HID_INTF_NUM         1
#define HID_INTF_NUM                3
#define CMSIS_DAP_HID_REPORT_DESC_SIZE 27
#define HIDRAW_INTERVAL             1
#else
#define CONFIG_HID_DESCRIPTOR_LEN   0
#define CONFIG_HID_INTF_NUM         0
#endif

#ifdef CONFIG_CHERRYDAP_USE_MSC
#define CONFIG_MSC_DESCRIPTOR_LEN CDC_ACM_DESCRIPTOR_LEN
#define CONFIG_MSC_INTF_NUM       1
#define MSC_INTF_NUM              (0x02 + 1)
#else
#define CONFIG_MSC_DESCRIPTOR_LEN 0
#define CONFIG_MSC_INTF_NUM       0
#define MSC_INTF_NUM              (0x02)
#endif

#ifdef CONFIG_USB_HS
#if DAP_PACKET_SIZE != 512
#error "DAP_PACKET_SIZE must be 512 in hs"
#endif
#else
#if DAP_PACKET_SIZE != 64
#error "DAP_PACKET_SIZE must be 64 in fs"
#endif
#endif

#define USBD_WINUSB_VENDOR_CODE 0x20

#define USBD_WEBUSB_ENABLE 0
#define USBD_BULK_ENABLE   1
#define USBD_WINUSB_ENABLE 1

/* WinUSB Microsoft OS 2.0 descriptor sizes */
#define WINUSB_DESCRIPTOR_SET_HEADER_SIZE  10
#define WINUSB_FUNCTION_SUBSET_HEADER_SIZE 8
#define WINUSB_FEATURE_COMPATIBLE_ID_SIZE  20

#define FUNCTION_SUBSET_LEN                160
#define DEVICE_INTERFACE_GUIDS_FEATURE_LEN 132

#define USBD_WINUSB_DESC_SET_LEN (WINUSB_DESCRIPTOR_SET_HEADER_SIZE + USBD_WEBUSB_ENABLE * FUNCTION_SUBSET_LEN + USBD_BULK_ENABLE * FUNCTION_SUBSET_LEN)

#define USBD_NUM_DEV_CAPABILITIES (USBD_WEBUSB_ENABLE + USBD_WINUSB_ENABLE)

#define USBD_WEBUSB_DESC_LEN 24
#define USBD_WINUSB_DESC_LEN 28

#define USBD_BOS_WTOTALLENGTH (0x05 +                                      \
                               USBD_WEBUSB_DESC_LEN * USBD_WEBUSB_ENABLE + \
                               USBD_WINUSB_DESC_LEN * USBD_WINUSB_ENABLE)

#define CONFIG_USBRX_RINGBUF_SIZE  (8 * 1024)

enum usb_string_index {
    USB_STRING_LANGID = 0,
    USB_STRING_MANUFACTURER,
    USB_STRING_PRODUCT,
    USB_STRING_SERIAL_NUMBER,
    USB_STRING_WEBUSB,
    USB_STRING_CMSIS_DAP_V2,
    USB_STRING_CMSIS_DAP_V1,
};

#ifdef CONFIG_DAP_HID
#define HID_DESC() \
    /************** Descriptor of Custom interface *****************/ \
    0x09,                          /* bLength: Interface Descriptor size */ \
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType: Interface descriptor type */ \
    HID_INTF_NUM,                  /* bInterfaceNumber: Number of Interface */ \
    0x00,                          /* bAlternateSetting: Alternate setting */ \
    0x02,                          /* bNumEndpoints */ \
    0x03,                          /* bInterfaceClass: HID */ \
    0x01,                          /* bInterfaceSubClass : 1=BOOT, 0=no boot */ \
    0x00,                          /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */ \
    USB_STRING_CMSIS_DAP_V1,       /* iInterface: Index of string descriptor */ \
    /******************** Descriptor of Custom HID ********************/ \
    0x09,                    /* bLength: HID Descriptor size */ \
    HID_DESCRIPTOR_TYPE_HID, /* bDescriptorType: HID */ \
    0x11,                    /* bcdHID: HID Class Spec release number */ \
    0x01, \
    0x00,                        /* bCountryCode: Hardware target country */ \
    0x01,                        /* bNumDescriptors: Number of HID class descriptors to follow */ \
    0x22,                        /* bDescriptorType */ \
    CMSIS_DAP_HID_REPORT_DESC_SIZE, /* wItemLength: Total length of Report descriptor */ \
    0x00, \
    /******************** Descriptor of Custom in endpoint ********************/ \
    0x07,                         /* bLength: Endpoint Descriptor size */ \
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */ \
    HID_IN_EP,                    /* bEndpointAddress: Endpoint Address (IN) */ \
    0x03,                         /* bmAttributes: Interrupt endpoint */ \
    WBVAL(HID_PACKET_SIZE),       /* wMaxPacketSize: 4 Byte max */ \
    HIDRAW_INTERVAL,              /* bInterval: Polling Interval */ \
    /******************** Descriptor of Custom out endpoint ********************/ \
    0x07,                         /* bLength: Endpoint Descriptor size */ \
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */ \
    HID_OUT_EP,                   /* bEndpointAddress: Endpoint Address (IN) */ \
    0x03,                         /* bmAttributes: Interrupt endpoint */ \
    WBVAL(HID_PACKET_SIZE),       /* wMaxPacketSize: 4 Byte max */ \
    HIDRAW_INTERVAL,              /* bInterval: Polling Interval */

extern struct usbd_endpoint hid_custom_in_ep;
extern struct usbd_endpoint hid_custom_out_ep;

extern const uint8_t cmsis_dap_hid_report_desc[CMSIS_DAP_HID_REPORT_DESC_SIZE];

#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usbrx_ringbuffer[CONFIG_USBRX_RINGBUF_SIZE];
extern USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usb_tmpbuffer[DAP_PACKET_SIZE];

extern const struct usb_descriptor cmsisdap_descriptor;
extern __ALIGN_BEGIN const uint8_t USBD_WinUSBDescriptorSetDescriptor[];
extern __ALIGN_BEGIN const uint8_t USBD_BinaryObjectStoreDescriptor[];
extern char *string_descriptors[];

extern struct usbd_interface dap_intf;
extern struct usbd_interface intf1;
extern struct usbd_interface intf2;
extern struct usbd_interface intf3;
extern struct usbd_interface hid_intf;

extern struct usbd_endpoint dap_out_ep;
extern struct usbd_endpoint dap_in_ep;
extern struct usbd_endpoint cdc_out_ep;
extern struct usbd_endpoint cdc_in_ep;
extern chry_ringbuffer_t g_usbrx;

__STATIC_INLINE void chry_dap_init(uint8_t busid, uint32_t reg_base)
{
    chry_ringbuffer_init(&g_usbrx, usbrx_ringbuffer, CONFIG_USBRX_RINGBUF_SIZE);

    DAP_Setup();

    usbd_desc_register(0, &cmsisdap_descriptor);

    /*!< winusb */
    usbd_add_interface(0, &dap_intf);
    usbd_add_endpoint(0, &dap_out_ep);
    usbd_add_endpoint(0, &dap_in_ep);

    /*!< cdc acm */
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &intf1));
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &intf2));
    usbd_add_endpoint(0, &cdc_out_ep);
    usbd_add_endpoint(0, &cdc_in_ep);
    
#ifdef CONFIG_DAP_HID
    /*!< hid */
    usbd_add_interface(0, usbd_hid_init_intf(0, &hid_intf, cmsis_dap_hid_report_desc, CMSIS_DAP_HID_REPORT_DESC_SIZE));
    usbd_add_endpoint(0, &hid_custom_in_ep);
    usbd_add_endpoint(0, &hid_custom_out_ep);
#endif

#ifdef CONFIG_CHERRYDAP_USE_MSC
    usbd_add_interface(0, usbd_msc_init_intf(0, &intf3, MSC_OUT_EP, MSC_IN_EP));
#endif
    extern void usbd_event_handler(uint8_t busid, uint8_t event);
    usbd_initialize(busid, reg_base, usbd_event_handler);
}

void chry_dap_handle(void);

void chry_dap_usb2uart_handle(void);

#ifdef __cplusplus
}
#endif

#endif
