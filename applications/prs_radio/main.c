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

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"

/* Libraries containing default Gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_prs.h"

/* Device initialization header */
#include "hal-config.h"

/* Enable TX_ACT signal through GPIO PD10 */
#define _PRS_CH_CTRL_SOURCESEL_RAC2 0x00000031UL
#define PRS_CH_CTRL_SOURCESEL_RAC2 (_PRS_CH_CTRL_SOURCESEL_RAC2 << 8)
#define _PRS_CH_CTRL_SIGSEL_RACPAEN 0x00000002UL
#define PRS_CH_CTRL_SIGSEL_RACPAEN (_PRS_CH_CTRL_SIGSEL_RACPAEN << 0)
#define _PRS_CH_CTRL_SIGSEL_RACTX 0x00000004UL
#define PRS_CH_CTRL_SIGSEL_RACTX (_PRS_CH_CTRL_SIGSEL_RACTX << 0)
#define _PRS_CH_CTRL_SIGSEL_RACRX 0x00000003UL
#define PRS_CH_CTRL_SIGSEL_RACRX (_PRS_CH_CTRL_SIGSEL_RACRX << 0)

/* RACPAEN Enable (TX_ACT) signal through GPIO PD10 */
#define TX_ACTIVE_PRS_SOURCE PRS_CH_CTRL_SOURCESEL_RAC2
#define TX_ACTIVE_PRS_SIGNAL PRS_CH_CTRL_SIGSEL_RACPAEN
#define TX_ACTIVE_PRS_CHANNEL 10
#define TX_ACTIVE_PRS_PORT gpioPortD
#define TX_ACTIVE_PRS_PIN 2

/* Enable RX_ACT signal through GPIO PD11 */
#define RX_ACTIVE_PRS_SOURCE PRS_CH_CTRL_SOURCESEL_RAC2
#define RX_ACTIVE_PRS_SIGNAL PRS_CH_CTRL_SIGSEL_RACRX
#define RX_ACTIVE_PRS_CHANNEL 11
#define RX_ACTIVE_PRS_PORT gpioPortD
#define RX_ACTIVE_PRS_PIN 3

/* BLE definitions */
#define MAX_ADVERTISERS 1
#define MAX_CONNECTIONS 4

static uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS)];

/* Bluetooth stack configuration parameters (see "UG136: Silicon Labs Bluetooth C Application Developer's Guide" for details on each parameter) */
static gecko_configuration_t config = {
  .config_flags = 0,                                   /* Check flag options from UG136 */
#if defined(FEATURE_LFXO) || defined(PLFRCO_PRESENT) || defined(LFRCO_PRESENT)
  .sleep.flags = SLEEP_FLAGS_DEEP_SLEEP_ENABLE,        /* Sleep is enabled */
#else
  .sleep.flags = 0,
#endif
  .bluetooth.max_connections = MAX_CONNECTIONS,        /* Maximum number of simultaneous connections */
  .bluetooth.max_advertisers = MAX_ADVERTISERS,        /* Maximum number of advertisement sets */
  .bluetooth.heap = bluetooth_stack_heap,              /* Bluetooth stack memory for connection management */
  .bluetooth.heap_size = sizeof(bluetooth_stack_heap), /* Bluetooth stack memory for connection management */
#if defined(FEATURE_LFXO)
  .bluetooth.sleep_clock_accuracy = 100,               /* Accuracy of the Low Frequency Crystal Oscillator in ppm. *
                                                       * Do not modify if you are using a module                  */
#elif defined(PLFRCO_PRESENT) || defined(LFRCO_PRESENT)
  .bluetooth.sleep_clock_accuracy = 500,               /* In case of internal RCO the sleep clock accuracy is 500 ppm */
#endif
  .gattdb = &bg_gattdb_data,                           /* Pointer to GATT database */
  .ota.flags = 0,                                      /* Check flag options from UG136 */
  .ota.device_name_len = 3,                            /* Length of the device name in OTA DFU mode */
  .ota.device_name_ptr = "OTA",                        /* Device name in OTA DFU mode */
  .pa.config_enable = 1,                               /* Set this to be a valid PA config */
#if defined(FEATURE_PA_INPUT_FROM_VBAT)
  .pa.input = GECKO_RADIO_PA_INPUT_VBAT,               /* Configure PA input to VBAT */
#else
  .pa.input = GECKO_RADIO_PA_INPUT_DCDC,               /* Configure PA input to DCDC */
#endif // defined(FEATURE_PA_INPUT_FROM_VBAT)
  .rf.flags = GECKO_RF_CONFIG_ANTENNA,                 /* Enable antenna configuration. */
  .rf.antenna = GECKO_RF_ANTENNA,                      /* Select antenna path! */
};

/* Flag for indicating DFU Reset must be performed */
static uint8_t boot_to_dfu = 0;

/**************************************************************************//**
 * @brief GPIO initialization
 *****************************************************************************/
static void initGpio(void)
{
  // Set RX/TX active pins
  GPIO_PinModeSet(TX_ACTIVE_PRS_PORT, TX_ACTIVE_PRS_PIN, gpioModePushPull, 0);
  GPIO_PinModeSet(RX_ACTIVE_PRS_PORT, RX_ACTIVE_PRS_PIN, gpioModePushPull, 0);

  /* Set up GPIO clock */
  CMU_ClockEnable(cmuClock_GPIO,true);
}

/**************************************************************************//**
 * @brief PRS initialization
 *****************************************************************************/
static void initPrs(void)
{
  /* Enable PRS clock */
  CMU_ClockEnable(cmuClock_PRS, true);

  /* Use RAC, PAEN as PRS source */
  PRS_SourceAsyncSignalSet( TX_ACTIVE_PRS_CHANNEL, PRS_CH_CTRL_SIGSEL_RACPAEN, TX_ACTIVE_PRS_SIGNAL);

  /* Use RAC, RX_ACT as PRS source */
  PRS_SourceAsyncSignalSet( RX_ACTIVE_PRS_CHANNEL, RX_ACTIVE_PRS_SOURCE, RX_ACTIVE_PRS_SIGNAL);

  /* Route output to PC01. No extra PRS logic needed here. */
  PRS_PinOutput(TX_ACTIVE_PRS_CHANNEL,prsTypeAsync, TX_ACTIVE_PRS_PORT , TX_ACTIVE_PRS_PIN);
  PRS_PinOutput(RX_ACTIVE_PRS_CHANNEL,prsTypeAsync, RX_ACTIVE_PRS_PORT , RX_ACTIVE_PRS_PIN);
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

  /* Set up GPIOs */
  initGpio();

  /* Set up PRS */
  initPrs();

  /* Initialize stack */
  gecko_init(&config);

  /* LED test */
  for(;;){
    /* Event pointer for handling events */
    struct gecko_cmd_packet* evt;

    /* Check for stack event. This is a blocking event listener. If you want non-blocking please see UG136. */
    evt = gecko_wait_event();

    /* Handle events */
    switch (BGLIB_MSG_ID(evt->header)) {
      /* This boot event is generated when the system boots up after reset.
       * Do not call any stack commands before receiving the boot event.
       * Here the system is set to start advertising immediately after boot procedure. */
      case gecko_evt_system_boot_id:
        /* Set advertising parameters. 100ms advertisement interval.
         * The first parameter is advertising set handle
         * The next two parameters are minimum and maximum advertising interval, both in
         * units of (milliseconds * 1.6).
         * The last two parameters are duration and maxevents left as default. */
        gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);

#if 0
        /* Change adverdising PHY */
        gecko_cmd_le_gap_set_advertise_phy(0,le_gap_phy_coded,le_gap_phy_coded);
#endif

        /* Start general advertising and enable connections. */
        gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
        break;

      case gecko_evt_le_connection_opened_id:

        break;

      case gecko_evt_le_connection_closed_id:

        /* Check if need to boot to OTA DFU mode */
        if (boot_to_dfu) {
          /* Enter to OTA DFU mode */
          gecko_cmd_system_reset(2);
        } else {
          /* Restart advertising after client has disconnected */
          gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
        }
        break;

      /* Events related to OTA upgrading
         ----------------------------------------------------------------------------- */

      /* Check if the user-type OTA Control Characteristic was written.
       * If ota_control was written, boot the device into Device Firmware Upgrade (DFU) mode. */
      case gecko_evt_gatt_server_user_write_request_id:

        if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_ota_control) {
          /* Set flag to enter to OTA mode */
          boot_to_dfu = 1;
          /* Send response to Write Request */
          gecko_cmd_gatt_server_send_user_write_response(
            evt->data.evt_gatt_server_user_write_request.connection,
            gattdb_ota_control,
            bg_err_success);

          /* Close connection to enter to DFU OTA mode */
          gecko_cmd_le_connection_close(evt->data.evt_gatt_server_user_write_request.connection);
        }
        break;

      /* Add additional event handlers as your application requires */

      default:
        break;
    }
  };
}
