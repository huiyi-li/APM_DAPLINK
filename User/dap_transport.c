#include "dap_transport.h"

#include <string.h>

#include "dap_main.h"

typedef struct {
    volatile uint16_t request_index_in;
    volatile uint16_t request_index_out;
    volatile uint16_t request_count_in;
    volatile uint16_t request_count_out;
    volatile uint8_t request_idle;
    volatile uint16_t response_index_in;
    volatile uint16_t response_index_out;
    volatile uint16_t response_count_in;
    volatile uint16_t response_count_out;
    volatile uint8_t response_in_flight;
    volatile uint8_t transfer_abort;
    uint8_t ep_out;
    uint8_t ep_in;
    uint16_t packet_size;
    uint8_t fixed_response_size;
    USB_MEM_ALIGNX uint8_t request[DAP_PACKET_COUNT][DAP_PACKET_SIZE];
    USB_MEM_ALIGNX uint8_t response[DAP_PACKET_COUNT][DAP_PACKET_SIZE];
    uint16_t response_size[DAP_PACKET_COUNT];
} dap_transport_state_t;

static USB_NOCACHE_RAM_SECTION dap_transport_state_t transports[2];

static volatile uint8_t usb_configured;
static volatile uint32_t usb_generation;
static uint8_t execute_request[DAP_PACKET_SIZE];

static dap_transport_state_t *transport_get(dap_transport_id_t id)
{
    return &transports[(uint32_t)id];
}

static uint32_t transport_lock(void)
{
    uint32_t state = __get_PRIMASK();

    __disable_irq();
    return state;
}

static void transport_unlock(uint32_t state)
{
    __DMB();
    if (state == 0U) {
        __enable_irq();
    }
}

static uint16_t outstanding(const dap_transport_state_t *transport)
{
    return (uint16_t)(transport->request_count_in - transport->response_count_out);
}

static uint16_t responses_pending(const dap_transport_state_t *transport)
{
    return (uint16_t)(transport->response_count_in - transport->response_count_out);
}

static void transport_reset(dap_transport_state_t *transport)
{
    transport->request_index_in = 0U;
    transport->request_index_out = 0U;
    transport->request_count_in = 0U;
    transport->request_count_out = 0U;
    transport->request_idle = 1U;
    transport->response_index_in = 0U;
    transport->response_index_out = 0U;
    transport->response_count_in = 0U;
    transport->response_count_out = 0U;
    transport->response_in_flight = 0U;
    transport->transfer_abort = 0U;
}

static void start_read(dap_transport_state_t *transport)
{
    uint32_t state;
    int ret;

    state = transport_lock();
    if ((usb_configured == 0U) || (transport->request_idle == 0U) ||
        (outstanding(transport) >= DAP_PACKET_COUNT)) {
        transport_unlock(state);
        return;
    }

    memset(transport->request[transport->request_index_in], 0, DAP_PACKET_SIZE);
    transport->request_idle = 0U;
    ret = usbd_ep_start_read(0, transport->ep_out,
                             transport->request[transport->request_index_in],
                             transport->packet_size);
    if (ret < 0) {
        transport->request_idle = 1U;
    }
    transport_unlock(state);
}

static void start_write(dap_transport_state_t *transport)
{
    uint32_t state;
    int ret;

    state = transport_lock();
    if ((usb_configured == 0U) || (transport->response_in_flight != 0U) ||
        (responses_pending(transport) == 0U)) {
        transport_unlock(state);
        return;
    }

    transport->response_in_flight = 1U;
    ret = usbd_ep_start_write(0, transport->ep_in,
                              transport->response[transport->response_index_out],
                              transport->response_size[transport->response_index_out]);
    if (ret < 0) {
        transport->response_in_flight = 0U;
    }
    transport_unlock(state);
}

static uint32_t process_one(dap_transport_state_t *transport)
{
    uint32_t generation;
    uint32_t state;
    uint16_t index;
    uint16_t pending;
    uint16_t response_size;

    if ((usb_configured == 0U) ||
        (transport->request_count_in == transport->request_count_out) ||
        (responses_pending(transport) >= DAP_PACKET_COUNT)) {
        return 0U;
    }

    generation = usb_generation;
    index = transport->request_index_out;
    pending = (uint16_t)(transport->request_count_in - transport->request_count_out);
    while ((pending != 0U) &&
           (transport->request[index][0] == ID_DAP_QueueCommands)) {
        transport->request[index][0] = ID_DAP_ExecuteCommands;
        index = (uint16_t)((index + 1U) % DAP_PACKET_COUNT);
        pending--;
    }

    if (transport->transfer_abort != 0U) {
        DAP_TransferAbort = 1U;
        transport->transfer_abort = 0U;
    }

    memcpy(execute_request,
           transport->request[transport->request_index_out],
           DAP_PACKET_SIZE);
    memset(transport->response[transport->response_index_in], 0, DAP_PACKET_SIZE);
    response_size = (uint16_t)DAP_ExecuteCommand(
        execute_request, transport->response[transport->response_index_in]);
    transport->response_size[transport->response_index_in] =
        transport->fixed_response_size ? transport->packet_size : response_size;

    state = transport_lock();
    if ((usb_configured == 0U) || (generation != usb_generation)) {
        transport_unlock(state);
        return 1U;
    }
    transport->request_index_out =
        (uint16_t)((transport->request_index_out + 1U) % DAP_PACKET_COUNT);
    transport->request_count_out++;
    transport->response_index_in =
        (uint16_t)((transport->response_index_in + 1U) % DAP_PACKET_COUNT);
    transport->response_count_in++;
    transport_unlock(state);

    start_write(transport);
    start_read(transport);
    return 1U;
}

void dap_transport_reset_all(void)
{
    uint32_t state = transport_lock();
    dap_transport_state_t *bulk = &transports[DAP_TRANSPORT_BULK];
    dap_transport_state_t *hid = &transports[DAP_TRANSPORT_HID];

    usb_configured = 0U;
    usb_generation++;
    DAP_TransferAbort = 1U;
    transport_reset(bulk);
    bulk->ep_out = DAP_OUT_EP;
    bulk->ep_in = DAP_IN_EP;
    bulk->packet_size = DAP_PACKET_SIZE;
    bulk->fixed_response_size = 0U;
    transport_reset(hid);
    hid->ep_out = HID_OUT_EP;
    hid->ep_in = HID_IN_EP;
    hid->packet_size = HID_PACKET_SIZE;
    hid->fixed_response_size = 1U;
    transport_unlock(state);
}

void dap_transport_set_configured(uint8_t configured)
{
    uint32_t state = transport_lock();

    usb_configured = configured ? 1U : 0U;
    transport_unlock(state);
}

void dap_transport_start_reads(void)
{
    start_read(&transports[DAP_TRANSPORT_BULK]);
    start_read(&transports[DAP_TRANSPORT_HID]);
}

void dap_transport_out_complete(dap_transport_id_t id, uint32_t nbytes)
{
    dap_transport_state_t *transport = transport_get(id);

    transport->request_idle = 1U;
    if ((usb_configured == 0U) || (nbytes == 0U)) {
        start_read(transport);
        return;
    }

    if (transport->request[transport->request_index_in][0] == ID_DAP_TransferAbort) {
        transport->transfer_abort = 1U;
        DAP_TransferAbort = 1U;
    } else {
        transport->request_index_in =
            (uint16_t)((transport->request_index_in + 1U) % DAP_PACKET_COUNT);
        transport->request_count_in++;
    }
    start_read(transport);
}

void dap_transport_in_complete(dap_transport_id_t id)
{
    dap_transport_state_t *transport = transport_get(id);
    uint32_t state = transport_lock();

    if (transport->response_in_flight != 0U) {
        transport->response_in_flight = 0U;
        transport->response_index_out =
            (uint16_t)((transport->response_index_out + 1U) % DAP_PACKET_COUNT);
        transport->response_count_out++;
    }
    transport_unlock(state);

    start_write(transport);
    start_read(transport);
}

void dap_transport_process(void)
{
    uint32_t progress;

    do {
        progress = process_one(&transports[DAP_TRANSPORT_BULK]);
        progress |= process_one(&transports[DAP_TRANSPORT_HID]);
    } while (progress != 0U);

    start_write(&transports[DAP_TRANSPORT_BULK]);
    start_write(&transports[DAP_TRANSPORT_HID]);
    dap_transport_start_reads();
}
