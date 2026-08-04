#ifndef DAP_TRANSPORT_H
#define DAP_TRANSPORT_H

#include <stdint.h>

typedef enum {
    DAP_TRANSPORT_BULK = 0,
    DAP_TRANSPORT_HID
} dap_transport_id_t;

void dap_transport_reset_all(void);
void dap_transport_set_configured(uint8_t configured);
void dap_transport_start_reads(void);
void dap_transport_out_complete(dap_transport_id_t id, uint32_t nbytes);
void dap_transport_in_complete(dap_transport_id_t id);
void dap_transport_process(void);

#endif
