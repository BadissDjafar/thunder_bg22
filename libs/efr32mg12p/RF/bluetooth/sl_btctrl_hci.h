/**
 * @brief Bluetooth controller HCI API.
 *
 * This interface is for external use.
 */
#ifndef _SL_BTCTRL_HCI_H_
#define _SL_BTCTRL_HCI_H_

#include <stdint.h>

//Initialize Vendor Specific Extensions
void sl_bthci_init_vs(void);

void sl_bthci_init_upper(void);

void hci_enableVendorSpecificDebugging(void);

void hci_debugEnable(void);

/**
 * The Bluetooth controller receives a HCI message fragment from the host.
 * The HCI transport can give the HCI message in fragments.
 * @param[in] data Pointer to the received data.
 * @param[in] len Length of the received data.
 * @param[in] lastFragment Indicate whether this is the last
 *       fragment of an HCI message (true / false).
 * @return  0 - success
 *         -1 - out of memory
 *         -2 - badly formatted message. */
int16_t sl_btctrl_hci_receive(uint8_t *data, int16_t len, bool lastFragment);

static inline int16_t hci_common_transport_receive(uint8_t *data, int16_t len, bool lastFragment)
{
  return sl_btctrl_hci_receive(data, len, lastFragment);
}

/**
 * The HCI transport has transmitted a message.
 * The HCI can transmit the next message after the transport
 * has given this indication. */
void sl_btctrl_hci_transmit_complete(uint32_t status);

static inline void hci_common_transport_transmit_complete(uint32_t status)
{
  sl_btctrl_hci_transmit_complete(status);
}

#endif // _SL_BTCTRL_HCI_H_
