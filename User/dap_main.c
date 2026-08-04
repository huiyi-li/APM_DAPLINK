#include "dap_main.h"
#include "dap_transport.h"
#include "DAP_config.h"
#include "DAP.h"
#include "bsp_cdc_uart.h"
#include "bsp_led.h"

#define USB_CONFIG_SIZE (9 + CMSIS_DAP_INTERFACE_SIZE + CDC_ACM_DESCRIPTOR_LEN + \
                        CONFIG_MSC_DESCRIPTOR_LEN + CONFIG_HID_DESCRIPTOR_LEN)
#define INTF_NUM        (2 + 1 + CONFIG_MSC_INTF_NUM + CONFIG_HID_INTF_NUM)

__ALIGN_BEGIN const uint8_t USBD_WinUSBDescriptorSetDescriptor[] = {
        WBVAL(WINUSB_DESCRIPTOR_SET_HEADER_SIZE), /* wLength */
        WBVAL(WINUSB_SET_HEADER_DESCRIPTOR_TYPE), /* wDescriptorType */
        0x00, 0x00, 0x03, 0x06, /* >= Win 8.1 */  /* dwWindowsVersion*/
        WBVAL(USBD_WINUSB_DESC_SET_LEN),          /* wDescriptorSetTotalLength */
#if (USBD_WEBUSB_ENABLE)
        WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE), // wLength
        WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE), // wDescriptorType
        0,                                         // bFirstInterface USBD_WINUSB_IF_NUM
        0,                                         // bReserved
        WBVAL(FUNCTION_SUBSET_LEN),                // wSubsetLength
        WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE),  // wLength
        WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE),  // wDescriptorType
        'W', 'I', 'N', 'U', 'S', 'B', 0, 0,        // CompatibleId
        0, 0, 0, 0, 0, 0, 0, 0,                    // SubCompatibleId
        WBVAL(DEVICE_INTERFACE_GUIDS_FEATURE_LEN), // wLength
        WBVAL(WINUSB_FEATURE_REG_PROPERTY_TYPE),   // wDescriptorType
        WBVAL(WINUSB_PROP_DATA_TYPE_REG_MULTI_SZ), // wPropertyDataType
        WBVAL(42),                                 // wPropertyNameLength
        'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
        'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
        'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
        WBVAL(80), // wProperty
DataLength
        '{', 0,
        '9', 0, '2', 0, 'C', 0, 'E', 0, '6', 0, '4', 0, '6', 0, '2', 0, '-', 0,
        '9', 0, 'C', 0, '7', 0, '7', 0, '-', 0,
        '4', 0, '6', 0, 'F', 0, 'E', 0, '-', 0,
        '9', 0, '3', 0, '3', 0, 'B', 0, '-',
        0, '3', 0, '1', 0, 'C', 0, 'B', 0, '9', 0, 'C', 0, '5', 0, 'A', 0, 'A', 0, '3', 0, 'B', 0, '9', 0,
        '}', 0, 0, 0, 0, 0
#endif

#if USBD_BULK_ENABLE
        WBVAL(WINUSB_FUNCTION_SUBSET_HEADER_SIZE), /* wLength */
        WBVAL(WINUSB_SUBSET_HEADER_FUNCTION_TYPE), /* wDescriptorType */
        0,                                         /* bFirstInterface USBD_BULK_IF_NUM*/
        0,                                         /* bReserved */
        WBVAL(FUNCTION_SUBSET_LEN),                /* wSubsetLength */
        WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_SIZE),  /* wLength */
        WBVAL(WINUSB_FEATURE_COMPATIBLE_ID_TYPE),  /* wDescriptorType */
        'W', 'I', 'N', 'U', 'S', 'B', 0, 0,        /* CompatibleId*/
        0, 0, 0, 0, 0, 0, 0, 0,                    /* SubCompatibleId*/
        WBVAL(DEVICE_INTERFACE_GUIDS_FEATURE_LEN), /* wLength */
        WBVAL(WINUSB_FEATURE_REG_PROPERTY_TYPE),   /* wDescriptorType */
        WBVAL(WINUSB_PROP_DATA_TYPE_REG_MULTI_SZ), /* wPropertyDataType */
        WBVAL(42),                                 /* wPropertyNameLength */
        'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
        'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
        'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0, 0,
        WBVAL(80), /* wPropertyDataLength */
        '{', 0,
        'C', 0, 'D', 0, 'B', 0, '3', 0, 'B', 0, '5', 0, 'A', 0, 'D', 0, '-', 0,
        '2', 0, '9', 0, '3', 0, 'B', 0, '-', 0,
        '4', 0, '6', 0, '6', 0, '3', 0, '-', 0,
        'A', 0, 'A', 0, '3', 0, '6', 0, '-',
        0, '1', 0, 'A', 0, 'A', 0, 'E', 0, '4', 0, '6', 0, '4', 0, '6', 0, '3', 0, '7', 0, '7', 0, '6', 0,
        '}', 0, 0, 0, 0, 0
#endif
};

__ALIGN_BEGIN const uint8_t USBD_BinaryObjectStoreDescriptor[] = {
        0x05,                         /* bLength */
        0x0f,                         /* bDescriptorType */
        WBVAL(USBD_BOS_WTOTALLENGTH), /* wTotalLength */
        USBD_NUM_DEV_CAPABILITIES,    /* bNumDeviceCaps */
#if (USBD_WEBUSB_ENABLE)
        USBD_WEBUSB_DESC_LEN,           /* bLength */
        0x10,                           /* bDescriptorType */
        USB_DEVICE_CAPABILITY_PLATFORM, /* bDevCapabilityType */
        0x00,                           /* bReserved */
        0x38, 0xB6, 0x08, 0x34,         /* PlatformCapabilityUUID */
        0xA9, 0x09, 0xA0, 0x47,
        0x8B, 0xFD, 0xA0, 0x76,
        0x88, 0x15, 0xB6, 0x65,
        WBVAL(0x0100), /* 1.00 */ /* bcdVersion */
        USBD_WINUSB_VENDOR_CODE,  /* bVendorCode */
        0,                        /* iLandingPage */
#endif
#if (USBD_WINUSB_ENABLE)
        USBD_WINUSB_DESC_LEN,           /* bLength */
        0x10,                           /* bDescriptorType */
        USB_DEVICE_CAPABILITY_PLATFORM, /* bDevCapabilityType */
        0x00,                           /* bReserved */
        0xDF, 0x60, 0xDD, 0xD8,         /* PlatformCapabilityUUID */
        0x89, 0x45, 0xC7, 0x4C,
        0x9C, 0xD2, 0x65, 0x9D,
        0x9E, 0x64, 0x8A, 0x9F,
        0x00, 0x00, 0x03, 0x06, /* >= Win 8.1 */ /* dwWindowsVersion*/
        WBVAL(USBD_WINUSB_DESC_SET_LEN),         /* wDescriptorSetTotalLength */
        USBD_WINUSB_VENDOR_CODE,                 /* bVendorCode */
        0,                                       /* bAltEnumCode */
#endif
};

#if 0 /* WebUSB custom report is not the CMSIS-DAP HID interface */
/*!< custom hid report descriptor */
const uint8_t hid_custom_report_desc[HID_CUSTOM_REPORT_DESC_SIZE] = {
        /* USER CODE BEGIN 0 */
        0x06, 0x00, 0xff, /* USAGE_PAGE (Vendor Defined Page 1) */
        0x09, 0x01, /* USAGE (Vendor Usage 1) */
        0xa1, 0x01, /* COLLECTION (Application) */
        0x85, 0x02, /*   REPORT ID (0x02) */
        0x09, 0x02, /*   USAGE (Vendor Usage 1) */
        0x15, 0x00, /*   LOGICAL_MINIMUM (0) */
        0x25, 0xff, /*LOGICAL_MAXIMUM (255) */
        0x75, 0x08, /*   REPORT_SIZE (8) */
        0x96, 0xff, 0x03, /*   REPORT_COUNT (1023) */
        0x81, 0x02, /*   INPUT (Data,Var,Abs) */
        /* <___________________________________________________> */
        0x85, 0x01, /*   REPORT ID (0x01) */
        0x09, 0x03, /*   USAGE (Vendor Usage 1) */
        0x15, 0x00, /*   LOGICAL_MINIMUM (0) */
        0x25, 0xff, /*   LOGICAL_MAXIMUM (255) */
        0x75, 0x08, /*   REPORT_SIZE (8) */
        0x96, 0xff, 0x03, /*   REPORT_COUNT (1023) */
        0x91, 0x02, /*   OUTPUT (Data,Var,Abs) */

        /* <___________________________________________________> */
        0x85, 0x03, /*   REPORT ID (0x03) */
        0x09, 0x04, /*   USAGE (Vendor Usage 1) */
        0x15, 0x00, /*   LOGICAL_MINIMUM (0) */
        0x25, 0xff, /*   LOGICAL_MAXIMUM (255) */
        0x75, 0x08, /*   REPORT_SIZE (8) */
        0x96, 0xff, 0x03, /*   REPORT_COUNT (1023) */
        0xb1, 0x02, /*   FEATURE (Data,Var,Abs) */
        /* USER CODE END 0 */
        0xC0 /*     END_COLLECTION	             */
};
#endif

USB_MEM_ALIGNX const uint8_t cmsis_dap_hid_report_desc[CMSIS_DAP_HID_REPORT_DESC_SIZE] = {
        0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01,
        0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08,
        0x95, HID_PACKET_SIZE, 0x09, 0x01, 0x81, 0x02,
        0x95, HID_PACKET_SIZE, 0x09, 0x01, 0x91, 0x02, 0xc0
};

static const uint8_t device_descriptor[] = {
        USB_DEVICE_DESCRIPTOR_INIT(USB_2_1, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01),
};

static const uint8_t config_descriptor[] = {
        USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
        /* Interface 0 */
        USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, USB_STRING_CMSIS_DAP_V2),
        /* Endpoint OUT 2 */
        USB_ENDPOINT_DESCRIPTOR_INIT(DAP_OUT_EP, USB_ENDPOINT_TYPE_BULK, DAP_PACKET_SIZE, 0x00),
        /* Endpoint IN 1 */
        USB_ENDPOINT_DESCRIPTOR_INIT(DAP_IN_EP, USB_ENDPOINT_TYPE_BULK, DAP_PACKET_SIZE, 0x00),
        CDC_ACM_DESCRIPTOR_INIT(0x01, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, DAP_PACKET_SIZE, 0x00),
    
#ifdef CONFIG_DAP_HID
        HID_DESC()
#endif
#ifdef CONFIG_CHERRYDAP_USE_MSC
        MSC_DESCRIPTOR_INIT(MSC_INTF_NUM, MSC_OUT_EP, MSC_IN_EP, DAP_PACKET_SIZE, 0x00),
#endif
};

static const uint8_t other_speed_config_descriptor[] = {
        USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
        /* Interface 0 */
        USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, USB_STRING_CMSIS_DAP_V2),
        /* Endpoint OUT 2 */
        USB_ENDPOINT_DESCRIPTOR_INIT(DAP_OUT_EP, USB_ENDPOINT_TYPE_BULK, DAP_PACKET_SIZE, 0x00),
        /* Endpoint IN 1 */
        USB_ENDPOINT_DESCRIPTOR_INIT(DAP_IN_EP, USB_ENDPOINT_TYPE_BULK, DAP_PACKET_SIZE, 0x00),
        CDC_ACM_DESCRIPTOR_INIT(0x01, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, DAP_PACKET_SIZE, 0x00),
#ifdef CONFIG_DAP_HID
        HID_DESC()
#endif
#ifdef CONFIG_CHERRYDAP_USE_MSC
        MSC_DESCRIPTOR_INIT(MSC_INTF_NUM, MSC_OUT_EP, MSC_IN_EP, DAP_PACKET_SIZE, 0x00),
#endif
};

char *string_descriptors[] = {
        (char[]) {0x09, 0x04},             /* Langid */
        "Ming",                            /* Manufacturer */
        "CMSIS-DAP",                      /* Product */
        "00000000000000000123456789ABCDEF", /* Serial Number */
        "CherryDAP WebUSB",
        "CMSIS-DAP v2",
        "CMSIS-DAP v1",
};

static const uint8_t device_quality_descriptor[] = {
        USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_1, 0x00, 0x00, 0x00, 0x01),
};

_Static_assert(sizeof(USBD_WinUSBDescriptorSetDescriptor) == USBD_WINUSB_DESC_SET_LEN,
               "WinUSB descriptor length mismatch");
_Static_assert(sizeof(USBD_BinaryObjectStoreDescriptor) == USBD_BOS_WTOTALLENGTH,
               "BOS descriptor length mismatch");
_Static_assert(sizeof(config_descriptor) == USB_CONFIG_SIZE,
               "configuration descriptor length mismatch");
_Static_assert(sizeof(other_speed_config_descriptor) == USB_CONFIG_SIZE,
               "other-speed descriptor length mismatch");
_Static_assert(sizeof(cmsis_dap_hid_report_desc) == CMSIS_DAP_HID_REPORT_DESC_SIZE,
               "CMSIS-DAP HID report descriptor length mismatch");

__WEAK const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void) speed;
    return device_descriptor;
}

__WEAK const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void) speed;
    return config_descriptor;
}

__WEAK const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void) speed;
    return device_quality_descriptor;
}

__WEAK const uint8_t *other_speed_config_descriptor_callback(uint8_t speed)
{
    (void) speed;
    return other_speed_config_descriptor;
}

__WEAK const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void) speed;

    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

volatile struct cdc_line_coding g_cdc_lincoding =
{
    .dwDTERate = 115200U,
    .bCharFormat = 0U,
    .bParityType = 0U,
    .bDataBits = 8U,
};
volatile uint8_t config_uart = 0;
volatile uint8_t config_uart_transfer = 0;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usbrx_ringbuffer[CONFIG_USBRX_RINGBUF_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usb_tmpbuffer[DAP_PACKET_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usb_cdc_in_buffer[DAP_PACKET_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t usb_cdc_out_buffer[DAP_PACKET_SIZE];
static volatile uint8_t usbrx_idle_flag = 0;
static volatile uint8_t usbtx_idle_flag = 0;
static volatile uint8_t cdc_led_hold_ms = 0U;

USB_NOCACHE_RAM_SECTION chry_ringbuffer_t g_usbrx;

void hid_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
    dap_transport_in_complete(DAP_TRANSPORT_HID);
}


void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void) busid;
    switch (event) {
        case USBD_EVENT_RESET:
            dap_transport_reset_all();
            chry_ringbuffer_reset(&g_usbrx);
            usbrx_idle_flag = 0;
            usbtx_idle_flag = 0;
            config_uart_transfer = 0;
            cdc_led_hold_ms = 0U;
            bsp_led_set(BSP_LED_USB, false);
            bsp_led_set(BSP_LED_SWD, false);
            bsp_led_set(BSP_LED_CDC, false);
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            dap_transport_reset_all();
            config_uart_transfer = 0U;
            bsp_led_set(BSP_LED_USB, false);
            bsp_led_set(BSP_LED_SWD, false);
            bsp_led_set(BSP_LED_CDC, false);
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            dap_transport_reset_all();
            usbtx_idle_flag = 1U;
            config_uart_transfer = 1U;
            bsp_led_set(BSP_LED_USB, true);
            dap_transport_set_configured(1U);
            dap_transport_start_reads();
            usbd_ep_start_read(0, CDC_OUT_EP, usb_tmpbuffer, DAP_PACKET_SIZE);
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}

void dap_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void) busid;
    (void) ep;
    dap_transport_out_complete(DAP_TRANSPORT_BULK, nbytes);
    bsp_led_set(BSP_LED_SWD, true);
}

void dap_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void) busid;
    (void) ep;
    (void) nbytes;
    dap_transport_in_complete(DAP_TRANSPORT_BULK);
}

void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void) busid;
    (void) ep;
    chry_ringbuffer_write(&g_usbrx, usb_tmpbuffer, nbytes);
    if (nbytes != 0U)
    {
        cdc_led_hold_ms = 50U;
        bsp_led_set(BSP_LED_CDC, true);
    }
    if (chry_ringbuffer_get_free(&g_usbrx) >= DAP_PACKET_SIZE) {
        usbd_ep_start_read(0, CDC_OUT_EP, usb_tmpbuffer, DAP_PACKET_SIZE);
    } else {
        usbrx_idle_flag = 1;
    }
}

void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void) busid;
    (void) ep;

    if ((nbytes % DAP_PACKET_SIZE) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(0, CDC_IN_EP, NULL, 0);
    } else {
        usbtx_idle_flag = 1U;
    }
}

static void hid_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    dap_transport_out_complete(DAP_TRANSPORT_HID, nbytes);
    bsp_led_set(BSP_LED_SWD, true);
}

struct usbd_endpoint dap_out_ep = {
        .ep_addr = DAP_OUT_EP,
        .ep_cb = dap_out_callback
};

struct usbd_endpoint dap_in_ep = {
        .ep_addr = DAP_IN_EP,
        .ep_cb = dap_in_callback
};

struct usbd_endpoint cdc_out_ep = {
        .ep_addr = CDC_OUT_EP,
        .ep_cb = usbd_cdc_acm_bulk_out
};

struct usbd_endpoint cdc_in_ep = {
        .ep_addr = CDC_IN_EP,
        .ep_cb = usbd_cdc_acm_bulk_in
};

#ifdef CONFIG_DAP_HID
struct usbd_endpoint hid_custom_in_ep = {
    .ep_addr = HID_IN_EP,
    .ep_cb = hid_in_callback
};

struct usbd_endpoint hid_custom_out_ep = {
    .ep_addr = HID_OUT_EP,
    .ep_cb = hid_out_callback
};
#endif

struct usbd_interface dap_intf;
struct usbd_interface intf1;
struct usbd_interface intf2;
struct usbd_interface intf3;
struct usbd_interface hid_intf;

struct usb_msosv2_descriptor msosv2_desc = {
        .vendor_code = USBD_WINUSB_VENDOR_CODE,
        .compat_id = USBD_WinUSBDescriptorSetDescriptor,
        .compat_id_len = USBD_WINUSB_DESC_SET_LEN,
};

struct usb_bos_descriptor bos_desc = {
        .string = USBD_BinaryObjectStoreDescriptor,
        .string_len = USBD_BOS_WTOTALLENGTH
};

const struct usb_descriptor cmsisdap_descriptor = {
        .device_descriptor_callback = device_descriptor_callback,
        .config_descriptor_callback = config_descriptor_callback,
        .device_quality_descriptor_callback = device_quality_descriptor_callback,
        .other_speed_descriptor_callback = other_speed_config_descriptor_callback,
        .string_descriptor_callback = string_descriptor_callback,
        .bos_descriptor = &bos_desc,
        .msosv2_descriptor = &msosv2_desc,
};

void chry_dap_handle(void)
{
    dap_transport_process();
}

void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding)
{
    (void) busid;
    (void) intf;
    if (memcmp(line_coding, (uint8_t *) &g_cdc_lincoding, sizeof(struct cdc_line_coding)) != 0) {
        memcpy((uint8_t *) &g_cdc_lincoding, line_coding, sizeof(struct cdc_line_coding));
        config_uart = 1;
        config_uart_transfer = 0;
    }
}

void usbd_cdc_acm_get_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding)
{
    (void) busid;
    (void) intf;
    memcpy(line_coding, (uint8_t *) &g_cdc_lincoding, sizeof(struct cdc_line_coding));
}

static bool configure_cdc_uart(const struct cdc_line_coding *line_coding)
{
    BSP_CDC_UART_CONFIG_T config;

    config.baud_rate = line_coding->dwDTERate;
    config.data_bits = line_coding->bDataBits;

    switch (line_coding->bParityType)
    {
        case 0U:
            config.parity = BSP_CDC_UART_PARITY_NONE;
            break;
        case 1U:
            config.parity = BSP_CDC_UART_PARITY_ODD;
            break;
        case 2U:
            config.parity = BSP_CDC_UART_PARITY_EVEN;
            break;
        default:
            return false;
    }

    switch (line_coding->bCharFormat)
    {
        case 0U:
            config.stop_bits = BSP_CDC_UART_STOP_BITS_1;
            break;
        case 1U:
            config.stop_bits = BSP_CDC_UART_STOP_BITS_1_5;
            break;
        case 2U:
            config.stop_bits = BSP_CDC_UART_STOP_BITS_2;
            break;
        default:
            return false;
    }

    return bsp_cdc_uart_configure(&config);
}

void chry_dap_usb2uart_handle(void)
{
    size_t size;
    size_t transferred;
    uint8_t *buffer;

    if (cdc_led_hold_ms != 0U)
    {
        cdc_led_hold_ms--;
        if (cdc_led_hold_ms == 0U)
        {
            bsp_led_set(BSP_LED_CDC, false);
        }
    }

    if (config_uart) {
        config_uart = 0;
        config_uart_transfer = configure_cdc_uart(
            (const struct cdc_line_coding *)&g_cdc_lincoding) ? 1U : 0U;
        usbtx_idle_flag = 1U;
    }

    if (config_uart_transfer == 0) {
        return;
    }

    /* UART RX to USB IN. */
    if (usbtx_idle_flag && (bsp_cdc_uart_rx_available() != 0U)) {
        size = bsp_cdc_uart_read(usb_cdc_in_buffer, DAP_PACKET_SIZE);
        if (size != 0U) {
            usbtx_idle_flag = 0U;
            cdc_led_hold_ms = 50U;
            bsp_led_set(BSP_LED_CDC, true);
            usbd_ep_start_write(0, CDC_IN_EP, usb_cdc_in_buffer, (uint32_t)size);
        }
    }

    /* USB OUT to UART TX. Consume only bytes accepted by the UART queue. */
    if (chry_ringbuffer_get_used(&g_usbrx)) {
        uint32_t contiguous_size = 0U;
        buffer = chry_ringbuffer_linear_read_setup(&g_usbrx, &contiguous_size);
        size = contiguous_size;
        if (size > bsp_cdc_uart_tx_free()) {
            size = bsp_cdc_uart_tx_free();
        }
        transferred = bsp_cdc_uart_write(buffer, size);
        if (transferred != 0U) {
            chry_ringbuffer_linear_read_done(&g_usbrx, (uint32_t)transferred);
        }
    }

    /* check whether usb rx ringbuffer have space to store */
    if (usbrx_idle_flag) {
        if (chry_ringbuffer_get_free(&g_usbrx) >= DAP_PACKET_SIZE) {
            usbrx_idle_flag = 0;
            usbd_ep_start_read(0, CDC_OUT_EP, usb_tmpbuffer, DAP_PACKET_SIZE);
        }
    }
}

#ifdef CONFIG_CHERRYDAP_USE_MSC
#define BLOCK_SIZE  512
#define BLOCK_COUNT 10

typedef struct
{
    uint8_t BlockSpace[BLOCK_SIZE];
} BLOCK_TYPE;

BLOCK_TYPE mass_block[BLOCK_COUNT];

void usbd_msc_get_cap(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
    *block_num = 1000; //Pretend having so many buffer,not has actually.
    *block_size = BLOCK_SIZE;
}
int usbd_msc_sector_read(uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if (sector < 10)
        memcpy(buffer, mass_block[sector].BlockSpace, length);
    return 0;
}

int usbd_msc_sector_write(uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if (sector < 10)
        memcpy(mass_block[sector].BlockSpace, buffer, length);
    return 0;
}
#endif
