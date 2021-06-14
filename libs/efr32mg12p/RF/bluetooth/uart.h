#ifndef UART_H
#define UART_H

#include "em_common.h"

#define hci_command_header_size     3   // opcode (2 bytes), length (1 byte)
#define hci_acl_data_header_size    4   // handle (2 bytes), length (2 bytes)

typedef uint32_t Ecode_t;

SL_PACK_START(1)
typedef struct {
  uint16_t opcode; /* HCI command opcode */
  uint8_t param_len; /* command parameter length */
} SL_ATTRIBUTE_PACKED hci_command_t;
SL_PACK_END()

SL_PACK_START(1)
typedef struct {
  uint16_t conn_handle; /* ACL connection handle */
  uint16_t length; /* Length of packet */
} SL_ATTRIBUTE_PACKED acl_packet_t;
SL_PACK_END()

enum hci_packet_type {
  hci_packet_type_command = 1,
  hci_packet_type_acl_data = 2
};

enum hci_uart_state {
  hci_uart_state_read_packet_type = 0,
  hci_uart_state_read_header = 1,
  hci_uart_state_read_data = 2
};

void uart_init(void);
int uart_read(uint8_t *data, uint16_t len);
Ecode_t uart_write(uint8_t *data, uint16_t len);
int uart_rx_buffered_length();
uint8_t* get_hci_rx_buffer(void);

#endif // UART_H
