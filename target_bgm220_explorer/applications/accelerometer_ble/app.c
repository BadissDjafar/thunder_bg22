/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
/* DEBUG */
#include <stdio.h>
#include "em_i2c.h"
/* DEBUG */

#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"
#include "app.h"

#include "em_common.h"

#include "bma400.h"      /* Sensor header */

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

/* Print boot message */
static void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt);

/* Flag for indicating DFU Reset must be performed */
static uint8_t boot_to_dfu = 0;

// If the notification is enabled or not
static uint8_t notific_enable;

/* Main application */
void appMain(gecko_configuration_t *pconfig)
{
#if DISABLE_SLEEP > 0
  pconfig->sleep.flags = 0;
#endif

  /* Initialize debug prints. Note: debug prints are off by default. See DEBUG_LEVEL in app.h */
  initLog();

  /* Initialize stack */
  gecko_init(pconfig);

  /* Debug */
  CMU_ClockEnable(cmuClock_GPIO,true);
  CMU_ClkOutPinConfig(0,cmuSelect_HFXO,1,gpioPortC,2);
  CMU_ClockEnable(cmuClock_GPIO,true);
  GPIO_PinModeSet(gpioPortB,2,gpioModePushPull,0); /* LED test */
  GPIO_PinOutClear(gpioPortB,2); /* Make sure the LED is cleared */

  while (1) {
    /* Event pointer for handling events */
    struct gecko_cmd_packet* evt;

    /* if there are no events pending then the next call to gecko_wait_event() may cause
     * device go to deep sleep. Make sure that debug prints are flushed before going to sleep */
    if (!gecko_event_pending()) {
      flushLog();
    }

    /* Check for stack event. This is a blocking event listener. If you want non-blocking please see UG136. */
    evt = gecko_wait_event();

    /* Handle events */
    switch (BGLIB_MSG_ID(evt->header)) {
      /* This boot event is generated when the system boots up after reset.
       * Do not call any stack commands before receiving the boot event.
       * Here the system is set to start advertising immediately after boot procedure. */
      case gecko_evt_system_boot_id:

        BMA400_Setup(); /* Setup and init BMA400 driver */

        bootMessage(&(evt->data.evt_system_boot));

        /* Set advertising parameters. 100ms advertisement interval.
         * The first parameter is advertising set handle
         * The next two parameters are minimum and maximum advertising interval, both in
         * units of (milliseconds * 1.6).
         * The last two parameters are duration and maxevents left as default. */
        gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);

        /* Start general advertising and enable connections. */
        gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);

        /* Read chip id */
        BMA400_chipid(eBMA400_Intfc_I2C0);

        break;

      case gecko_evt_le_connection_opened_id:

        notific_enable = 0;
        gecko_cmd_hardware_set_soft_timer(2000, 0, 0); /* 1s/20 that is 50 ms */

        BMA400_set_power_mode(BMA400_NORMAL_MODE, eBMA400_Intfc_I2C0);

        break;

      case gecko_evt_hardware_soft_timer_id:
      {
        struct bma400_sensor_data stAccel; /* acceleration data */
        int8_t i8Res;
        uint8_t accel_buffer[3];

        GPIO_PinOutSet(gpioPortB,2); /* the LED is Set */
        if( BMA400_OK != BMA400_get_accel_data(BMA400_DATA_ONLY,&stAccel,eBMA400_Intfc_I2C0))
        {
          printf(".\n\r");
        }

        if (stAccel.x > 0)
          accel_buffer[0] = stAccel.x / 16;
        else
          accel_buffer[0] = (-stAccel.x) / 16;

        if (stAccel.y > 0)
          accel_buffer[1] = stAccel.y / 16;
        else
          accel_buffer[1] = (-stAccel.y) / 16;

        if (stAccel.z > 0)
          accel_buffer[2] = stAccel.z / 16;
        else
          accel_buffer[2] = (-stAccel.z) / 16;

        GPIO_PinOutClear(gpioPortB,2); /* the LED is Clear */
        gecko_cmd_gatt_write_characteristic_value(gattdb_acceleration, 0, 3, accel_buffer);

        if (notific_enable == 1)
        {
          gecko_cmd_gatt_server_send_characteristic_notification(0xFF, gattdb_acceleration, 3, accel_buffer);
        }

        break;
      }

      case gecko_evt_le_connection_closed_id:

        printLog("connection closed, reason: 0x%2.2x\r\n", evt->data.evt_le_connection_closed.reason);

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
  }
}

/* Print stack version and local Bluetooth address as boot message */
static void bootMessage(struct gecko_msg_system_boot_evt_t *bootevt)
{
#if DEBUG_LEVEL
  bd_addr local_addr;
  int i;

  printLog("stack version: %u.%u.%u\r\n", bootevt->major, bootevt->minor, bootevt->patch);
  local_addr = gecko_cmd_system_get_bt_address()->address;

  printLog("local BT device address: ");
  for (i = 0; i < 5; i++) {
    printLog("%2.2x:", local_addr.addr[5 - i]);
  }
  printLog("%2.2x\r\n", local_addr.addr[0]);
#endif
}

/*******************************************************************************
 *                             I2C0 Interrupt handler
 *****************************************************************************/

/* This has to live here for now because it needs to stay generic enough. For a
 * lack of a more general I2C bus module */
void I2C0_IRQHandler(void)
{
  BMA400_I2C_ISR_Handler(eBMA400_Intfc_I2C0);
  return;
}
