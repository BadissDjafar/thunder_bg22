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
#include "ble-configuration.h"
#include "board_features.h"

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"

/* Libraries containing default Gecko configuration values */
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_letimer.h"

/* Device initialization header */
#include "hal-config.h"

#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif

/* ########################################################################## */
/*                                     Macros                                 */
/* ########################################################################## */
/* Desired frequency in Hz */
#define OUT_FREQ 1000

/* Desired repeat count */
#define REPEAT_COUNT 20

/* Port and pin definitions for the explorer kit */
#define BGM220_EXPKIT_CLK_HFXO_OUT_PORT (gpioPortC)
#define BGM220_EXPKIT_CLK_HFXO_OUT_PIN  (2)

#define BGM220_EXPKIT_CLK_LFXO_OUT_PORT (gpioPortC)
#define BGM220_EXPKIT_CLK_LFXO_OUT_PIN  (1)

#define BGM220_EXPKIT_LED_OUT_PORT (gpioPortB)
#define BGM220_EXPKIT_LED_OUT_PIN  (2)

#define BGM220_EXPKIT_LETIMER_OUT_PORT (gpioPortA)
#define BGM220_EXPKIT_LETIMER_OUT_PIN  (0)

/* ########################################################################## */
/*                     Local functions declaration                            */
/* ########################################################################## */
static void initGpio(void);
static void initClock(void);
static void initLetimer(void);

/* Use either LE Timer or regular Timers. Could use the watchdog .... TBD */
/* ########################################################################## */
/*                             Application code                               */
/* ########################################################################## */
int main(void)
{
  /* Initialize device */
  initMcu();
  /* Initialize board */
  initBoard();
  /* Initialize application */
  initApp();
  initVcomEnable();

  initClock();
  initGpio();
  initLetimer();

  // Enter low energy state
  for(;;)
  {
    EMU_EnterEM1();
  }
}

/* ########################################################################## */
/*                     Local functions definitions                            */
/* ########################################################################## */
static void initClock(void)
{
  CMU_LFXOInit_TypeDef lfxoInit = CMU_LFXOINIT_DEFAULT;

  // Select LFXO for the LETIMER
  CMU_LFXOInit(&lfxoInit);
  CMU_ClockSelectSet(cmuClock_EM23GRPACLK, cmuSelect_LFXO);
}

static void initGpio(void)
{
  /* Debug */
  CMU_ClockEnable(cmuClock_GPIO,true);
  /* Set sytem clock as output on pin */
  CMU_ClkOutPinConfig( 1,
                       cmuSelect_LFXO,
                       1,
                       BGM220_EXPKIT_CLK_LFXO_OUT_PORT,
                       BGM220_EXPKIT_CLK_LFXO_OUT_PIN );

  CMU_ClkOutPinConfig( 0,
                       cmuSelect_HFXO,
                       1,
                       BGM220_EXPKIT_CLK_HFXO_OUT_PORT,
                       BGM220_EXPKIT_CLK_HFXO_OUT_PIN );

  /* LED reset */
  GPIO_PinModeSet(BGM220_EXPKIT_LED_OUT_PORT,BGM220_EXPKIT_LED_OUT_PIN,gpioModePushPull,0); /* LED test */
  GPIO_PinOutClear(BGM220_EXPKIT_LED_OUT_PORT,BGM220_EXPKIT_LED_OUT_PIN); /* Make sure the LED is cleared */

  /* Configure PA6 (Expansion Header 14) as output */
  GPIO_PinModeSet(BGM220_EXPKIT_LETIMER_OUT_PORT, BGM220_EXPKIT_LETIMER_OUT_PIN, gpioModePushPull, 0);

  /* enable LETIMER0 clocking */
  CMU_ClockEnable(cmuClock_LETIMER0, true);

  return;
}

static void initLetimer(void)
{
  LETIMER_Init_TypeDef letimerInit = LETIMER_INIT_DEFAULT;

  // Calculate the top value (frequency) based on clock source
  uint32_t topValue = CMU_ClockFreqGet(cmuClock_LETIMER0) / OUT_FREQ;

  // Reload top on underflow, set output, and run in one-shot mode
  letimerInit.comp0Top = true;
  letimerInit.topValue = topValue;
  letimerInit.ufoa0 = letimerUFOAToggle;
  letimerInit.repMode = letimerRepeatOneshot;

  // Enable LETIMER0 output0 on PA6
  GPIO->LETIMERROUTE[0].ROUTEEN = GPIO_LETIMER_ROUTEEN_OUT0PEN;
  GPIO->LETIMERROUTE[0].OUT0ROUTE = \
      (BGM220_EXPKIT_LETIMER_OUT_PORT << _GPIO_LETIMER_OUT0ROUTE_PORT_SHIFT) \
      | (BGM220_EXPKIT_LETIMER_OUT_PIN << _GPIO_LETIMER_OUT0ROUTE_PIN_SHIFT);

  // repeat 10
  LETIMER_RepeatSet(LETIMER0, 0, REPEAT_COUNT);

  // Initialize and enable LETIMER
  LETIMER_Init(LETIMER0, &letimerInit);
}
