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




/***************************************************************************//**
 * @file main.c
 * @brief This project demonstrates output compare in EM2 using the LETIMER.
 * After 1 second an output pin is set high (see README)
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *******************************************************************************
 * # Evaluation Quality
 * This code has been minimally tested to ensure that it builds and is suitable 
 * as a demonstration for evaluation purposes only. This code will be maintained
 * at the sole discretion of Silicon Labs.
 ******************************************************************************/

#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_letimer.h"
#include "bsp.h"

// Desired frequency in Hz
#define OUT_FREQ 1000

// Desired repeat count
#define REPEAT_COUNT 10

/**************************************************************************//**
 * @brief GPIO initialization
 *****************************************************************************/
void initGpio(void)
{
  // Configure PA6 (Expansion Header 14) as output
  GPIO_PinModeSet(gpioPortA, 6, gpioModePushPull, 0);
}

/**************************************************************************//**
 * @brief
 *    CMU initialization
 *****************************************************************************/
void initCmu(void)
{
  // Enable clock to GPIO and LETIMER0
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_LETIMER0, true);
}

/**************************************************************************//**
 * @brief Clock initialization
 *****************************************************************************/
void initClock(void)
{
  CMU_LFXOInit_TypeDef lfxoInit = CMU_LFXOINIT_DEFAULT;

  // Select LFXO for the LETIMER
  CMU_LFXOInit(&lfxoInit);
  CMU_ClockSelectSet(cmuClock_EM23GRPACLK, cmuSelect_LFXO);
}

/**************************************************************************//**
 * @brief LETIMER initialization
 *****************************************************************************/
void initLetimer(void)
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
      (gpioPortA << _GPIO_LETIMER_OUT0ROUTE_PORT_SHIFT) \
      | (6 << _GPIO_LETIMER_OUT0ROUTE_PIN_SHIFT);

  // repeat 10
  LETIMER_RepeatSet(LETIMER0, 0, REPEAT_COUNT);

  // Initialize and enable LETIMER
  LETIMER_Init(LETIMER0, &letimerInit);
}

/**************************************************************************//**
 * @brief  Main function
 *****************************************************************************/
int main(void)
{
  // Chip errata
  CHIP_Init();

  // Initializations
  initCmu();
  initClock();
  initGpio();
  initLetimer();

  // Enter low energy state
  while (1)
  {
    EMU_EnterEM2(true);
  }
}
