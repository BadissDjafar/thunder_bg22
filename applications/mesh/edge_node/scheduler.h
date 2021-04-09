/***************************************************************************//**
 * @file  scheduler.h
 * @brief Scheduler header file
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

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "native_gecko.h"

/***************************************************************************//**
 * @defgroup Scheduler Scheduler Module
 * @brief Scheduler model implementation
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup Scheduler
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Scheduler initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where scheduler model is initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t scheduler_init(uint16_t element);

/***************************************************************************//**
 * Handling of mesh scheduler events.
 * It handles:
 *  - scheduler_server_action_changed
 *
 * @param[in] pEvt  Pointer to incoming scheduler event.
 ******************************************************************************/
void handle_scheduler_server_events(struct gecko_cmd_packet *pEvt);

/** @} (end addtogroup Scheduler) */

#endif /* SCHEDULER_H */
