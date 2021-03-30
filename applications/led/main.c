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

/* Libraries containing default Gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"

/* Device initialization header */
#include "hal-config.h"

/**
 * @brief  Main function
 */
int main(void)
{
  uint8_t idx;
  uint8_t led_array[BSP_LED_COUNT][2]=BSP_LED_INIT;/* 2 is for both the port and the pin number */

  /* Initialize device */
  initMcu();
  /* Initialize board */
  initBoard();
  /* Initialize application */
  initApp();
  initVcomEnable();

  /* Set up GPIO clock */
  CMU_ClockEnable(cmuClock_GPIO,true);

  /* LED test */
  if (1 == BSP_LED_PRESENT)
  {
    for( idx = 0 ; idx < BSP_LED_COUNT; idx++ )
    {
      GPIO_PinModeSet(led_array[idx][0],led_array[idx][1],gpioModePushPull,0);
      GPIO_PinOutSet(led_array[idx][0],led_array[idx][1]);
    }
  }
  for(;;){};
}

/** @} (end addtogroup app) */
/** @} (end addtogroup Application) */
