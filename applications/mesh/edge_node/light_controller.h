/***************************************************************************//**
 * @file  light_controller.h
 * @brief Light controller header file
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

#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include "native_gecko.h"
#include "mesh_generic_model_capi_types.h"

/***************************************************************************//**
 * @defgroup LC Light Controller Module
 * @brief LC Module Implementation
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup LC
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * LC initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where LC model is initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t lc_init(uint16_t element);

/***************************************************************************//**
 * This function is getter for current light controller mode.
 *
 * @return  current light controller mode
 ******************************************************************************/
uint8_t lc_get_mode(void);

/***************************************************************************//**
 * Light Controller state update on power up sequence.
 *
 * @param[in] element    Index of the element.
 * @param[in] onpowerup  Value of OnPowerUp state.
 ******************************************************************************/
void lc_onpowerup_update(uint16_t element, uint8_t onpowerup);

/***************************************************************************//**
 * Handling of mesh light controller events.
 * It handles:
 *  - lc_server_mode_updated
 *  - lc_server_om_updated
 *  - lc_server_light_onoff_updated
 *  - lc_server_occupancy_updated
 *  - lc_server_ambient_lux_level_updated
 *  - lc_server_linear_output_updated
 *  - lc_setup_server_set_property
 *
 * @param[in] pEvt  Pointer to incoming light controller event.
 ******************************************************************************/
void handle_lc_server_events(struct gecko_cmd_packet *pEvt);

/***************************************************************************//**
 * Handling of light controller timer events.
 *
 * @param[in] evt  Pointer to incoming event.
 ******************************************************************************/
void handle_lc_timer_evt(struct gecko_cmd_packet *evt);

/***************************************************************************//**
 * @defgroup LC_GenericOnOff Light Controller Generic OnOff Server
 * @brief LC Generic OnOff Server Implementation
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup LC_GenericOnOff
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * This function process the requests for the LC generic on/off model.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] server_addr    Address the message was sent to.
 * @param[in] appkey_index   The application key index used in encrypting the request.
 * @param[in] request        Pointer to the request structure.
 * @param[in] transition_ms  Requested transition time (in milliseconds).
 * @param[in] delay_ms       Delay time (in milliseconds).
 * @param[in] request_flags  Message flags. Bitmask of the following:
 *                           - Bit 0: Nonrelayed. If nonzero indicates
 *                                    a response to a nonrelayed request.
 *                           - Bit 1: Response required. If nonzero client
 *                                    expects a response from the server.
 ******************************************************************************/
void lc_onoff_request(uint16_t model_id,
                      uint16_t element_index,
                      uint16_t client_addr,
                      uint16_t server_addr,
                      uint16_t appkey_index,
                      const struct mesh_generic_request *request,
                      uint32_t transition_ms,
                      uint16_t delay_ms,
                      uint8_t request_flags);

/***************************************************************************//**
 * This function is a handler for LC generic on/off change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
void lc_onoff_change(uint16_t model_id,
                     uint16_t element_index,
                     const struct mesh_generic_state *current,
                     const struct mesh_generic_state *target,
                     uint32_t remaining_ms);

/***************************************************************************//**
 * This function is a handler for LC generic on/off recall event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
void lc_onoff_recall(uint16_t model_id,
                     uint16_t element_index,
                     const struct mesh_generic_state *current,
                     const struct mesh_generic_state *target,
                     uint32_t transition_ms);

/** @} (end addtogroup LC_GenericOnOff) */
/** @} (end addtogroup LC) */

#endif /* LIGHT_CONTROLLER_H */
