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

/* sytem headers */
#include <stdio.h>

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

/* Device initialization header */
#include "hal-config.h"

#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif

/* retarget serial */
#include "retargetserial.h"

/* ########################################################################## */
/*                                     Macros                                 */
/* ########################################################################## */

/* System control block */
#define CPUID_ADDRESS (0xE000ED00) /* cpuid register address */
#define ID_PFR0 (0xE000ED40) /* Processor Feature Register 0 */
#define ID_PFR1 (0xE000ED44) /* Processor Feature Register 1 */
#define ID_DFR0 (0xE000ED48) /* Debug Feature Register 0 */
#define ID_AFR0 0xE000ED4C  /* Auxiliary Feature Register 0 */
#define ID_MMFR0 0xE000ED50 /* Memory Model Feature Register 0 */
#define ID_MMFR1 0xE000ED54 /* Memory Model Feature Register 1 */
#define ID_MMFR2 0xE000ED58 /* Memory Model Feature Register 2 */
#define ID_MMFR3 0xE000ED5C /* Memory Model Feature Register 3 */
#define ID_ISAR0 0xE000ED60 /* Instruction Set Attribute Register 0 */
#define ID_ISAR1 0xE000ED64 /* Instruction Set Attribute Register 1s */

/* Floating point extension */
#define FPCCR_ADDRESS (0xE000EF34) /* status and control */
#define FPCAR_ADDRESS (0xE000EF38) /* address reg */
#define FPCDSCR_ADDRESS (0xE000EF3C) /* default statuc control */

/* ########################################################################## */
/*                     Local functions declaration                            */
/* ########################################################################## */
#if 0
static void static_fundtion(void);
#endif

/* Use either LE Timer or regular Timers. Could use the watchdog .... TBD */
/* ########################################################################## */
/*                             Application code                               */
/* ########################################################################## */
int main(void)
{
  volatile uint32_t reg_value = 0x00000000;

  /* Initialize device */
  initMcu();
  /* Initialize board */
  initBoard();
  /* Initialize application */
  initApp();
  initVcomEnable();

  /* Read CPUID registers and output on UART */

  /* Initialize debug prints. Note: debug prints are off by default. See DEBUG_LEVEL in app.h */
  RETARGET_SerialInit();
  RETARGET_SerialFlush();

  printf("CPUID Application\n\r");
  reg_value = *((uint32_t*)CPUID_ADDRESS);
  printf("CPUID -> 0x%.8x\n\r",reg_value);

  printf("FPU Info\n\r");
  reg_value = *((uint32_t*)FPCCR_ADDRESS);
  printf("FPCCR -> 0x%.8x\n\r",reg_value);
  reg_value = *((uint32_t*)FPCAR_ADDRESS);
  printf("FPCAR -> 0x%.8x\n\r",reg_value);
  reg_value = *((uint32_t*)FPCDSCR_ADDRESS);
  printf("FPCDSCR -> 0x%.8x\n\r",reg_value);

  // Enter low energy state
  for(;;)
  {
    EMU_EnterEM1();
  }
}

/* ########################################################################## */
/*                     Local functions definitions                            */
/* ########################################################################## */
#if 0
static void static_fundtion(void)
{
}
#endif
