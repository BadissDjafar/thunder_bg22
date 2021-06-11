/***************************************************************************//**
 * @file
 * @brief Silicon Labs Empty Example Project
 *
 * This example demonstrates the bare minimum needed for a Blue Gecko C application
 * that allows Over-the-Air Device Firmware Upgrading (OTA DFU). The application
 * starts advertising after boot and restarts advertising after a connection is closed.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* Board headers */
#include "init_mcu.h"
#include "init_board.h"
#include "init_app.h"
#include "board_features.h"

/* Librarie and general includes */
#include "sl_btctrl_hci.h"
#include "sl_hci_common_transport.h"
#include "sl_btctrl_packets.h"
#include "sl_btctrl_linklayer.h"
#include "sl_btctrl_config.h"

/* HCI and UART */
#include "sl_btctrl_hci_uart.h"

/* Device initialization header */
#include "hal-config.h"

#ifndef SL_BT_CONFIG_MAX_CONNECTIONS
#define SL_BT_CONFIG_MAX_CONNECTIONS 1
#endif
#ifndef SL_BT_CONFIG_USER_ADVERTISERS
#define SL_BT_CONFIG_USER_ADVERTISERS 1
#endif
#ifndef SL_BT_CONFIG_ACCEPT_LIST_SIZE
#define SL_BT_CONFIG_ACCEPT_LIST_SIZE 1
#endif

static bool sl_btctrl_testhw_init(void)
{
  //For internal testing to bypass initialization
  return false;
}

void sl_bt_controller_init(void)
{
  if(sl_btctrl_testhw_init() == false){
    sl_btctrl_init_mem(SL_BT_CONTROLLER_BUFFER_MEMORY);
    sl_btctrl_init_ll();
    sl_btctrl_init_basic(SL_BT_CONFIG_MAX_CONNECTIONS,SL_BT_CONFIG_USER_ADVERTISERS,SL_BT_CONFIG_ACCEPT_LIST_SIZE);
  }
}

/**
 * @brief  Main function
 */
int main(void)
{
  /* Initialize device */
  initMcu();
  /* Initialize board */
  initBoard();
  /* Initialize application */
  initApp();
  initVcomEnable();
  
  /* Init LL */
  sl_bt_controller_init();
  
  for(;;){
  };
}
