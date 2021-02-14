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

/* Libraries containing default Gecko configuration values */
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_timer.h"

/* Device initialization header */
#include "hal-config.h"

#if 0
#include "ustimer.h" /* us timer */
#endif

#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif

/* ########################################################################## */
/*                                     Macros                                 */
/* ########################################################################## */
/* Desired frequency in Hz */
#define OUT_FREQ 1

/* Port and pin definitions for the explorer kit */
#define TB22_EXPKIT_CLK_HFXO_OUT_PORT (gpioPortC)
#define TB22_EXPKIT_CLK_HFXO_OUT_PIN  (2)

#define TB22_EXPKIT_CLK_LFXO_OUT_PORT (gpioPortC)
#define TB22_EXPKIT_CLK_LFXO_OUT_PIN  (1)

#define TB22_EXPKIT_LED_OUT_PORT (gpioPortB)
#define TB22_EXPKIT_LED_OUT_PIN  (0)

#define TB22_EXPKIT_TIMER_OUT_PORT (gpioPortA)
#define TB22_EXPKIT_TIMER_OUT_PIN  (8)

/* ########################################################################## */
/*                     Local functions declaration                            */
/* ########################################################################## */
static void initGpio(void);
static void initClock(void);
static void initTimer(void);

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
  initTimer();

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
  CMU_ClockEnable(cmuClock_TIMER0, true);
}

static void initGpio(void)
{
  /* Debug */
  CMU_ClockEnable(cmuClock_GPIO,true);
  /* Set sytem clock as output on pin */
  CMU_ClkOutPinConfig( 1,
                       cmuSelect_LFXO,
                       1,
                       TB22_EXPKIT_CLK_LFXO_OUT_PORT,
                       TB22_EXPKIT_CLK_LFXO_OUT_PIN );

  CMU_ClkOutPinConfig( 0,
                       cmuSelect_HFXO,
                       1,
                       TB22_EXPKIT_CLK_HFXO_OUT_PORT,
                       TB22_EXPKIT_CLK_HFXO_OUT_PIN );

  /* LED reset */
  GPIO_PinModeSet(TB22_EXPKIT_LED_OUT_PORT,TB22_EXPKIT_LED_OUT_PIN,gpioModePushPull,0); /* LED test */
  GPIO_PinOutClear(TB22_EXPKIT_LED_OUT_PORT,TB22_EXPKIT_LED_OUT_PIN); /* Make sure the LED is cleared */

  /* Configure PA6 (Expansion Header 14) as output */
  GPIO_PinModeSet(TB22_EXPKIT_TIMER_OUT_PORT, TB22_EXPKIT_TIMER_OUT_PIN, gpioModePushPull, 0);

  /* enable LETIMER0 clocking */
  CMU_ClockEnable(cmuClock_LETIMER0, true);

  return;
}

static void initTimer(void)
{
  uint32_t timerFreq = 0;

  /* Initialize the timer */
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;

  /* Configure TIMER0 Compare/Capture for output compare */
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;

  timerInit.prescale = timerPrescale1;
  timerInit.enable = false;
  timerCCInit.mode = timerCCModeCompare;
  timerCCInit.cmoa = timerOutputActionToggle;

  // configure, but do not start timer
  TIMER_Init(TIMER0, &timerInit);

  // Route Timer0 CC0 output to PA6
  GPIO->TIMERROUTE[0].ROUTEEN  = GPIO_TIMER_ROUTEEN_CC0PEN;
  GPIO->TIMERROUTE[0].CC0ROUTE = (TB22_EXPKIT_TIMER_OUT_PORT << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
                    | (TB22_EXPKIT_TIMER_OUT_PIN << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);

  TIMER_InitCC(TIMER0, 0, &timerCCInit);

  // Set Top value
  // Note each overflow event constitutes 1/2 the signal period
  timerFreq = CMU_ClockFreqGet(cmuClock_TIMER0)/(timerInit.prescale + 1);
  int topValue = timerFreq / (2*OUT_FREQ) - 1;
  TIMER_TopSet (TIMER0, topValue);

  /* Start the timer */
  TIMER_Enable(TIMER0, true);

  TIMER_IntEnable(TIMER0,TIMER_IEN_CC0);

  /* Start the timer */
  TIMER_Enable(TIMER0, true);
  NVIC_ClearPendingIRQ(TIMER0_IRQn);
  NVIC_EnableIRQ(TIMER0_IRQn);
}

void TIMER0_IRQHandler(void)
{
  if( 0x00000000 != (TIMER_IntGet(TIMER0)&TIMER_IF_CC0) )
  {
    GPIO_PinOutToggle(TB22_EXPKIT_LED_OUT_PORT,TB22_EXPKIT_LED_OUT_PIN); /* Make sure the LED is cleared */
    TIMER_IntClear(TIMER0, TIMER_IF_CC0);
  }

  return;
}
