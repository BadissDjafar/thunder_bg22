#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <em_device.h>
#include <em_gpio.h>
#include <em_core.h>
#include "sl_uartdrv_instances.h"
#include "sl_uartdrv_usart_vcom_config.h"
#include "fifo.h"
#include "sl_btctrl_linklayer.h"
#include "sl_hci_common_transport.h"
#include "../inc/sl_btctrl_hci_uart.h"
#include "uart.h"

#define uart_rx_buffer_size         64 // size needs to be even number

static UARTDRV_Handle_t handle = NULL;
static fifo_define(uart_rx_buffer, uart_rx_buffer_size);

static struct {
  struct fifo *rx_buffer;
} uart;

static void start_rx(UARTDRV_Handle_t handle);
static void tx_callback(UARTDRV_Handle_t handle, Ecode_t status, uint8_t *buffer, UARTDRV_Count_t count);

void uart_init(void)
{
  fifo_init(&uart_rx_buffer, 0);
  uart.rx_buffer = &uart_rx_buffer;
  handle = sl_uartdrv_get_default();
  start_rx(handle);
  USART_Enable(SL_UARTDRV_USART_VCOM_PERIPHERAL, usartEnable);
  sli_btctrl_set_interrupt_priorities();
}

#if SL_UARTDRV_USART_VCOM_PERIPHERAL_NO == 0
void USART0_RX_IRQHandler()
#elif SL_UARTDRV_USART_VCOM_PERIPHERAL_NO == 1
void USART1_RX_IRQHandler()
#elif SL_UARTDRV_USART_VCOM_PERIPHERAL_NO == 2
void USART2_RX_IRQHandler()
#elif SL_UARTDRV_USART_VCOM_PERIPHERAL_NO == 3
void USART3_RX_IRQHandler()
#else
#error Unsupported VCOM device
#endif
{
  int irq = USART_IntGet(SL_UARTDRV_USART_VCOM_PERIPHERAL);
  if (irq & USART_IF_RXDATAV) {
    USART_IntClear(SL_UARTDRV_USART_VCOM_PERIPHERAL, USART_IF_RXDATAV);
  }
}

bool rx_callback(unsigned int channel, unsigned int sequenceNo, void *userParam)
{
  (void)channel;
  (void)sequenceNo;
  (void)userParam;
  uint8_t *dma_tail;
  int size = fifo_size(uart.rx_buffer) / 2;
  // reserve next half of fifo for dma
  fifo_dma_reserve(uart.rx_buffer, &dma_tail, size);
  // dma transfer is ready, update the tail of fifo
  fifo_dma_set_tail(uart.rx_buffer, dma_tail);
  return true;
}

static void start_rx(UARTDRV_Handle_t handle)
{
  int size = fifo_size(uart.rx_buffer) / 2;
  // reserve first half of fifo for dma
  fifo_dma_reserve(uart.rx_buffer, NULL, size);
  uint8_t *dst0 = fifo_buffer(uart.rx_buffer);
  uint8_t *dst1 = fifo_buffer(uart.rx_buffer) + size;
  uint8_t *src = (uint8_t *)&handle->peripheral.uart->RXDATA;
  DMADRV_PeripheralMemoryPingPong(handle->rxDmaCh, handle->rxDmaSignal,
                                  dst0, dst1, src, true, size, dmadrvDataSize1, rx_callback, NULL);
}

static void update_buffer_status(UARTDRV_Handle_t handle)
{
  int remaining = 0;
  uint8_t *dma_tail;
  DMADRV_TransferRemainingCount(handle->rxDmaCh, &remaining);
  fifo_dma_reserve(uart.rx_buffer, &dma_tail, 0);
  fifo_dma_set_tail(uart.rx_buffer, dma_tail - remaining);
}

int uart_read(uint8_t *data, uint16_t len)
{
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  update_buffer_status(handle);

  CORE_EXIT_ATOMIC();
  len = fifo_read(uart.rx_buffer, data, len);
  return len;
}

/* Peek how much data buffered in rx that can be immediately read out */
int uart_rx_buffered_length()
{
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  update_buffer_status(handle);

  CORE_EXIT_ATOMIC();
  return fifo_length(uart.rx_buffer);
}

static void tx_callback(UARTDRV_Handle_t handle, Ecode_t status, uint8_t *buffer, UARTDRV_Count_t count)
{
  (void)handle;
  (void)buffer;
  (void)count;
  hci_common_transport_transmit_complete(status);
}

Ecode_t uart_write(uint8_t *data, uint16_t len)
{
  Ecode_t status;
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_ATOMIC();

  status = UARTDRV_Transmit(handle, data, len, tx_callback);

  CORE_EXIT_ATOMIC();
  return status;
}
