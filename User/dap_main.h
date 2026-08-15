#ifndef DAP_MAIN_H
#define DAP_MAIN_H

#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_msc.h"
#include "usbd_hid.h"
#include "chry_ringbuffer.h"
#include "DAP_config.h"
#include "DAP.h"

#define HID_IN_EP   0x81
#define HID_OUT_EP  0x01

#define DAP_HID_INT_EP 0x81

#define DAP_IN_EP  0x82
#define DAP_OUT_EP 0x02

#define CDC_IN_EP  0x83
#define CDC_OUT_EP 0x03
#define CDC_INT_EP 0x84

#define CDC1_IN_EP  0x85
#define CDC1_OUT_EP 0x05
#define CDC1_INT_EP 0x86

#define MSC_IN_EP  0x87
#define MSC_OUT_EP 0x07

#define CONFIG_DAP_HID


#define USBD_VID           0x0D28
#define USBD_PID           0x0204
#define USBD_MAX_POWER     500
#define USBD_LANGID_STRING 1033

#define CMSIS_DAP_INTERFACE_SIZE (9 + 7 + 7)

#ifdef CONFIG_DAP_HID
#define HID_PACKET_SIZE             512
#define CONFIG_HID_DESCRIPTOR_LEN   (9 + 9 + 7 + 7)
#define CONFIG_HID_INTF_NUM         1
#define HID_CUSTOM_REPORT_DESC_SIZE 56
#define HID_INTF_NUM                5
#define HIDRAW_INTERVAL             4
#else
#define CONFIG_HID_DESCRIPTOR_LEN   0
#define CONFIG_HID_INTF_NUM         0
#endif

#define CONFIG_CHERRYDAP_USE_MSC

#ifdef CONFIG_CHERRYDAP_USE_MSC
#define CONFIG_MSC_DESCRIPTOR_LEN CDC_ACM_DESCRIPTOR_LEN
#define CONFIG_MSC_INTF_NUM       1
#define MSC_INTF_NUM              6
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

#define CONFIG_UARTRX_RINGBUF_SIZE (8 * 1024)
#define CONFIG_USBRX_RINGBUF_SIZE  (8 * 1024)


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
    0,                             /* iInterface: Index of string descriptor */ \
    /******************** Descriptor of Custom HID ********************/ \
    0x09,                    /* bLength: HID Descriptor size */ \
    HID_DESCRIPTOR_TYPE_HID, /* bDescriptorType: HID */ \
    0x11,                    /* bcdHID: HID Class Spec release number */ \
    0x01, \
    0x00,                        /* bCountryCode: Hardware target country */ \
    0x01,                        /* bNumDescriptors: Number of HID class descriptors to follow */ \
    0x22,                        /* bDescriptorType */ \
    HID_CUSTOM_REPORT_DESC_SIZE, /* wItemLength: Total length of Report descriptor */ \
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

extern const uint8_t hid_custom_report_desc[HID_CUSTOM_REPORT_DESC_SIZE];

extern uint8_t HID_read_buffer[];
extern uint8_t HID_write_buffer[];

void HID_Handle();


#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t uartrx_ringbuffer[CONFIG_UARTRX_RINGBUF_SIZE];
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
extern struct usbd_interface intf4;
extern struct usbd_interface intf5;
extern struct usbd_interface intf6;
extern struct usbd_interface hid_intf;

extern struct usbd_endpoint dap_out_ep;
extern struct usbd_endpoint dap_in_ep;
extern struct usbd_endpoint cdc_out_ep;
extern struct usbd_endpoint cdc_in_ep;
extern struct usbd_endpoint cdc_out_ep1;
extern struct usbd_endpoint cdc_in_ep1;
extern struct usbd_endpoint hid_int_ep;

extern chry_ringbuffer_t g_uartrx;
extern chry_ringbuffer_t g_uartrx1;
extern chry_ringbuffer_t g_usbrx;
extern chry_ringbuffer_t g_usbrx1;
extern volatile struct cdc_line_coding g_cdc1_lincoding;
extern USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t uartrx_ringbuffer1[CONFIG_UARTRX_RINGBUF_SIZE];
extern USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usbrx_ringbuffer1[CONFIG_USBRX_RINGBUF_SIZE];

void usbd_cdc_acm_bulk_out1(uint8_t busid, uint8_t ep, uint32_t nbytes);
void usbd_cdc_acm_bulk_in1(uint8_t busid, uint8_t ep, uint32_t nbytes);
void chry_dap_usb2uart_uart_config_callback1(struct cdc_line_coding *line_coding);
void chry_dap_usb2uart_uart_send_bydma1(uint8_t *data, uint16_t len);
void chry_dap_usb2uart_uart_send_complete1(uint32_t size);

__STATIC_INLINE void chry_dap_init(uint8_t busid, uint32_t reg_base)
{
    chry_ringbuffer_init(&g_uartrx, uartrx_ringbuffer, CONFIG_UARTRX_RINGBUF_SIZE);
    chry_ringbuffer_init(&g_usbrx, usbrx_ringbuffer, CONFIG_USBRX_RINGBUF_SIZE);

    /* Second CDC (CDC1): USART3 (bsp_cdc_uart) bridge. */
    chry_ringbuffer_init(&g_uartrx1, uartrx_ringbuffer1, CONFIG_UARTRX_RINGBUF_SIZE);
    chry_ringbuffer_init(&g_usbrx1, usbrx_ringbuffer1, CONFIG_USBRX_RINGBUF_SIZE);

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

    /*!< cdc acm 2 (CDC1 -> USART3) */
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &intf3));
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &intf4));
    usbd_add_endpoint(0, &cdc_out_ep1);
    usbd_add_endpoint(0, &cdc_in_ep1);
    
#ifdef CONFIG_DAP_HID
    /*!< hid */
    usbd_add_interface(0, usbd_hid_init_intf(0, &hid_intf, hid_custom_report_desc, HID_CUSTOM_REPORT_DESC_SIZE));
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

void chry_dap_usb2uart_uart_config_callback(struct cdc_line_coding *line_coding);

void chry_dap_usb2uart_uart_send_bydma(uint8_t *data, uint16_t len);

void chry_dap_usb2uart_uart_send_complete(uint32_t size);

#ifdef __cplusplus
}
#endif

#endif