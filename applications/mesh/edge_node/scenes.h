/***************************************************************************//**
 * @file  scenes.h
 * @brief Scenes module header file
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SCENES_H
#define SCENES_H

#include "native_gecko.h"

/***************************************************************************//**
 * @defgroup Scenes Scenes Module
 * @brief Scenes Implementation
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup Scenes
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Scenes initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where scenes models are initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t scenes_init(uint16_t element);

/***************************************************************************//**
 * Handling of mesh scenes events.
 * It handles:
 *  - scene_server_get
 *  - scene_server_register_get
 *  - scene_server_recall
 *  - scene_server_publish
 *  - scene_setup_server_store
 *  - scene_setup_server_delete
 *  - scene_setup_server_publish
 *
 * @param[in] pEvt  Pointer to incoming scenes event.
 ******************************************************************************/
void handle_scenes_server_events(struct gecko_cmd_packet *pEvt);

/** @} (end addtogroup Scenes) */

#endif /* SCENES_H */
