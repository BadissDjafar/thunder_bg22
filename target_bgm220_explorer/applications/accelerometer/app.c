/***************************************************************************//**
 * @file app.c
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

/* ########################################################################## */
/*                           System includes                                  */
/* ########################################################################## */
#include <stdio.h>  /* standard io defs */
#include <string.h> /* string defns, memcpy, etc ... */

#include "bg_types.h"     /* Gecko SDK type definitions */
#include "native_gecko.h" /* Bluetooth stack headers */

#include "em_emu.h" /* Energy management */

/* ########################################################################## */
/*                          Non system includes                               */
/* ########################################################################## */
#include "app.h" /* Application header */

#include "bma400.h"      /* Sensor header */

/* ########################################################################## */
/*                             Macros Typedef                                 */
/* ########################################################################## */
/* Earth's gravity in m/s^2 */
#define GRAVITY_EARTH     (9.80665f)

/* Interface selection enums */
typedef enum demo_idx {
  eCMD_DEMO_BMA400_SELFTEST = 0, /* Selftest demo */
  eCMD_DEMO_BMA400_CHIPID,       /* Read chip id */
  eCMD_DEMO_BMA400_ACCEL,        /* Accelerometer demo */
  eCMD_DEMO_BMA400_ACCEL_BLE,    /* Accelerometer + BLE demo*/
  eCMD_DEMO_BMA400_TOTAL
} eCMD_DEMO_Idx;

struct bma400_sensor_data stAccel; /* acceleration data */

/* ########################################################################## */
/*                       Local functions declarations                         */
/* ########################################################################## */
static eCMD_DEMO_Idx eDemo;
static void CMD_bkgnd(void);
static float lsb_to_ms2(int16_t accel_data, uint8_t g_range, uint8_t bit_width);

/* ########################################################################## */
/*                             Application code                               */
/* ########################################################################## */
void appMain(void)
{
  uint8_t u8Data;
  int8_t i8Res;
  float x, y, z;

  BMA400_Setup(); /* Setup and init BMA400 driver */

  /* Initialise demo control variable */
  eDemo = eCMD_DEMO_BMA400_TOTAL;
  x = (float)0.0;
  y = (float)0.0;
  z = (float)0.0;

  /* Main program loop */
  for(;;)
  {
    /* Command interpreter task */
    CMD_bkgnd();

    switch(eDemo)
    {
      case eCMD_DEMO_BMA400_SELFTEST:
        i8Res=BMA400_perform_self_test(eBMA400_Intfc_I2C0);
        if( i8Res == BMA400_OK )
        {
          printf("[Result Self Test] SUCCESS -> %d\r\n",i8Res);
        }
        else
        {
          printf("[Result Self Test] FAIL -> %d\r\n",i8Res);
        }
        printf("#\r\n");
        break;
      case eCMD_DEMO_BMA400_CHIPID:
        u8Data=BMA400_chipid(eBMA400_Intfc_I2C0);
        printf("[Read - CHIP ID] -> 0x%.2x\r\n",u8Data);
        printf("#\r\n");
        break;
      case eCMD_DEMO_BMA400_ACCEL:
        BMA400_set_power_mode(BMA400_NORMAL_MODE, eBMA400_Intfc_I2C0);
        i8Res=BMA400_get_accel_data(BMA400_DATA_ONLY,&stAccel,eBMA400_Intfc_I2C0);

        /* 12-bit accelerometer at range 2G */
        x = lsb_to_ms2(stAccel.x, 2, 12);
        y = lsb_to_ms2(stAccel.y, 2, 12);
        z = lsb_to_ms2(stAccel.z, 2, 12);

        printf("[Accel X] -> <0x%.2x;%.2f> |[Accel Y] -> <0x%.2x;%.2f> |[Accel Z] -> <0x%.2x;%.2f>\r\n",stAccel.x,x,stAccel.y,y,stAccel.z,z);
        printf("#\r\n");
        break;
      case eCMD_DEMO_BMA400_ACCEL_BLE:
        /* Back to simple uart RX */
        /* TBD : Need to think about a better power strategy */
        break;
      case eCMD_DEMO_BMA400_TOTAL:
        /* Back to simple uart RX */
        /* TBD : Need to think about a better power strategy */
        break;
      default:
        printf("invalid command\r\n");
        break;
    }

    /* Transition back to idle, use the eCMD_DEMO_BMA400_TOTAL value */
    eDemo = eCMD_DEMO_BMA400_TOTAL;
  }
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

/* ########################################################################## */
/*                        Local functions definitions                         */
/* ########################################################################## */
static void CMD_bkgnd(void)
{
  int key;

  /* Command interpreter */
  key = RETARGET_ReadChar();

  /* Command description banner */
  if( key == '\r' )
  {
    printf("-> Which demo would you like to launch ?\n\r - Use key 'a' for acceleration \n\r - Use key 'b' for acceleration over BLE\n\r - Use key 'c' for reading the chip id\n\r - Use key 'f' to flush the console \n\r - Use key 's' for self test\r\n(# indicates the demo ended successfully)\r\n");
  }

  /* clean up traces */
  if( key == 'f' )
  {
    /* flush UART */
    printf("Flushing\r\n");
    RETARGET_SerialFlush();
  }

  if( key == 's' )
  {
    eDemo = eCMD_DEMO_BMA400_SELFTEST;
    printf("Starting BMA400 selftest demo\r\n");
  }

  if( key == 'c' )
  {
    eDemo = eCMD_DEMO_BMA400_CHIPID;
    printf("Starting BMA400 chip id demo\r\n");
  }

  if( key == 'a' )
  {
    eDemo = eCMD_DEMO_BMA400_ACCEL;
    printf("Starting BMA400 acceleration demo\r\n");
  }

  if( key == 'b' )
  {
    eDemo = eCMD_DEMO_BMA400_ACCEL_BLE;
    printf("Starting BMA400 acceleration sent over BLE demo\r\n");
  }

  return;
}

static float lsb_to_ms2(int16_t accel_data, uint8_t g_range, uint8_t bit_width)
{
  float accel_ms2;
  int16_t half_scale;

  half_scale = 1 << (bit_width - 1);
  accel_ms2 = (GRAVITY_EARTH * accel_data * g_range) / half_scale;

  return accel_ms2;
}
