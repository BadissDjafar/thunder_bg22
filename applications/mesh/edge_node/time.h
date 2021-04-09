/***************************************************************************//**
 * @file  time.h
 * @brief Time header file
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

#ifndef TIME_H
#define TIME_H

#include "native_gecko.h"

/***************************************************************************//**
 * @defgroup Time Time Module
 * @brief Time model implementation
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup Time
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Time initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where time model is initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t time_init(uint16_t element);

/***************************************************************************//**
 * Handling of mesh time events.
 * It handles:
 *  - time_server_time_updated
 *  - time_server_time_zone_offset_updated
 *  - time_server_tai_utc_delta_updated
 *  - time_server_time_role_updated
 *
 * @param[in] pEvt  Pointer to incoming time event.
 ******************************************************************************/
void handle_time_server_events(struct gecko_cmd_packet *pEvt);

/** @} (end addtogroup Time) */

#endif /* TIME_H */
