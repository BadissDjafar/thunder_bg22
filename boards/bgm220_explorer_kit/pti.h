/***************************************************************************//**
 * @file
 * @brief Functions and data related to PTI
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
#include <stdint.h>

#if !defined(RADIO_PTI_INIT)
#define RADIO_PTI_INIT                                                     \
  {                                                                        \
    RADIO_PTI_MODE_UART,    /* Simplest output mode is UART mode */        \
    1600000,                /* Choose 1.6 MHz for best compatibility */    \
    0,                      /* WSTK uses location x for DOUT */            \
    gpioPortC,              /* Get the port for this loc */                \
    4,                      /* Get the pin, location should match above */ \
    0,                      /* WSTK uses location x for DCLK */            \
    gpioPortB,              /* Get the port for this loc */                \
    11,                     /* Get the pin, location should match above */ \
    0,                      /* WSTK uses location x for DFRAME */          \
    gpioPortC,              /* Get the port for this loc */                \
    5,                      /* Get the pin, location should match above */ \
  }
#endif

#if !defined(RAIL_PTI_CONFIG)
#define RAIL_PTI_CONFIG                                                    \
  {                                                                        \
    RAIL_PTI_MODE_UART,     /* Simplest output mode is UART mode */        \
    1600000,                /* Choose 1.6 MHz for best compatibility */    \
    0,                      /* WSTK uses location x for DOUT */            \
    gpioPortC,              /* Get the port for this loc */                \
    4,                      /* Get the pin, location should match above */ \
    0,                      /* WSTK uses location x for DCLK */            \
    gpioPortB,              /* Get the port for this loc */                \
    11,                     /* Get the pin, location should match above */ \
    0,                      /* WSTK uses location x for DFRAME */          \
    gpioPortC,              /* Get the port for this loc */                \
    5,                      /* Get the pin, location should match above */ \
  }
#endif

#if HAL_PTI_ENABLE && !defined(RAIL_PTI_CONFIG)

// Series 2 devices do not have LOC settings, so HWCONF will not define the
// following macros for those devices. The RAIL structure, on the other hand,
// stayed the same for backwards compatibility reasons. RAIL will ignore the
// LOC setting for series 2 devices, so we give them any value here to avoid
// compilation errors.
#if defined(_SILICON_LABS_32B_SERIES_2)
#define BSP_PTI_DFRAME_LOC 0
#define BSP_PTI_DOUT_LOC 0
#define BSP_PTI_DCLK_LOC 0
#endif

#if HAL_PTI_MODE == HAL_PTI_MODE_SPI
#define RAIL_PTI_CONFIG                                                   \
  {                                                                       \
    RAIL_PTI_MODE_SPI,     /* SPI mode */                                 \
    HAL_PTI_BAUD_RATE,     /* Baud rate */                                \
    BSP_PTI_DOUT_LOC,      /* DOUT location */                            \
    BSP_PTI_DOUT_PORT,     /* Get the port for this loc */                \
    BSP_PTI_DOUT_PIN,      /* Get the pin, location should match above */ \
    BSP_PTI_DCLK_LOC,      /* DCLK location */                            \
    BSP_PTI_DCLK_PORT,     /* Get the port for this loc */                \
    BSP_PTI_DCLK_PIN,      /* Get the pin, location should match above */ \
    BSP_PTI_DFRAME_LOC,    /* DFRAME location */                          \
    BSP_PTI_DFRAME_PORT,   /* Get the port for this loc */                \
    BSP_PTI_DFRAME_PIN,    /* Get the pin, location should match above */ \
  }
#elif HAL_PTI_MODE == HAL_PTI_MODE_UART
#define RAIL_PTI_CONFIG                                                   \
  {                                                                       \
    RAIL_PTI_MODE_UART,    /* UART mode */                                \
    HAL_PTI_BAUD_RATE,     /* Baud rate */                                \
    BSP_PTI_DOUT_LOC,      /* DOUT location */                            \
    BSP_PTI_DOUT_PORT,     /* Get the port for this loc */                \
    BSP_PTI_DOUT_PIN,      /* Get the pin, location should match above */ \
    0,                     /* No DCLK in UART mode */                     \
    0,                     /* No DCLK in UART mode */                     \
    0,                     /* No DCLK in UART mode */                     \
    BSP_PTI_DFRAME_LOC,    /* DFRAME location */                          \
    BSP_PTI_DFRAME_PORT,   /* Get the port for this loc */                \
    BSP_PTI_DFRAME_PIN,    /* Get the pin, location should match above */ \
  }
#elif HAL_PTI_MODE == HAL_PTI_MODE_UART_ONEWIRE
#define RAIL_PTI_CONFIG                                                        \
  {                                                                            \
    RAIL_PTI_MODE_UART_ONEWIRE, /* UART onewire mode */                        \
    HAL_PTI_BAUD_RATE,          /* Baud rate */                                \
    BSP_PTI_DOUT_LOC,           /* DOUT location */                            \
    BSP_PTI_DOUT_PORT,          /* Get the port for this loc */                \
    BSP_PTI_DOUT_PIN,           /* Get the pin, location should match above */ \
    0,                          /* No DCLK in UART onewire mode */             \
    0,                          /* No DCLK in UART onewire mode */             \
    0,                          /* No DCLK in UART onewire mode */             \
    0,                          /* No DFRAME in UART onewire mode */           \
    0,                          /* No DFRAME in UART onewire mode */           \
    0,                          /* No DFRAME in UART onewire mode */           \
  }
#else
  #error "Invalid PTI mode (HAL_PTI_MODE)"
#endif
#endif

uint8_t configEnablePti(void);
