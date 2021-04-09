/***************************************************************************//**
 * @file  scenes.c
 * @brief Scenes module implementation
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

/* Own header */
#include "scenes.h"

/* C Standard Library headers */
#include <stdio.h>

#ifdef ENABLE_LOGGING
#define log(...) printf(__VA_ARGS__)
#else
#define log(...)
#endif

/***************************************************************************//**
 * @addtogroup Scenes
 * @{
 ******************************************************************************/

/*******************************************************************************
 * Scenes initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where scenes models are initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t scenes_init(uint16_t element)
{
  // Initialize scenes server models
  uint16_t result = gecko_cmd_mesh_scene_server_init(element)->result;
  if (result) {
    log("mesh_scene_server_init failed, code 0x%x\r\n", result);
    return result;
  }

  result = gecko_cmd_mesh_scene_setup_server_init(element)->result;
  if (result) {
    log("mesh_scene_setup_server_init failed, code 0x%x\r\n", result);
  }

  return result;
}

/***************************************************************************//**
 * Handling of scene server get event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene server get event.
 ******************************************************************************/
static void handle_scene_server_get_event(
  struct gecko_msg_mesh_scene_server_get_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_server_get_id, client_address=%u, \
appkey_index=%u\r\n",
      pEvt->client_address,
      pEvt->appkey_index);
}

/***************************************************************************//**
 * Handling of scene server regiter get event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene server register get event.
 ******************************************************************************/
static void handle_scene_server_register_get_event(
  struct gecko_msg_mesh_scene_server_register_get_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_server_register_get_id, client_address=%u, \
appkey_index=%u\r\n",
      pEvt->client_address,
      pEvt->appkey_index);
}

/***************************************************************************//**
 * Handling of scene server recall event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene server recall event.
 ******************************************************************************/
static void handle_scene_server_recall_event(
  struct gecko_msg_mesh_scene_server_recall_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_server_recall_id, client_address=%u, \
appkey_index=%u, selected_scene=%u, transition_time=%lu\r\n",
      pEvt->client_address,
      pEvt->appkey_index,
      pEvt->selected_scene,
      pEvt->transition_time);
}

/***************************************************************************//**
 * Handling of scene server publish event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene server publish event.
 ******************************************************************************/
static void handle_scene_server_publish_event(
  struct gecko_msg_mesh_scene_server_publish_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_server_publish_id, period_ms=%lu\r\n",
      pEvt->period_ms);
}

/***************************************************************************//**
 * Handling of scene setup server store event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene setup server store event.
 ******************************************************************************/
static void handle_scene_setup_server_store_event(
  struct gecko_msg_mesh_scene_setup_server_store_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_setup_server_store_id, client_address=%u, \
appkey_index=%u, scene_id=%u\r\n",
      pEvt->client_address,
      pEvt->appkey_index,
      pEvt->scene_id);
}

/***************************************************************************//**
 * Handling of scene setup server delete event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene setup server delete event.
 ******************************************************************************/
static void handle_scene_setup_server_delete_event(
  struct gecko_msg_mesh_scene_setup_server_delete_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_setup_server_delete_id, client_address=%u, \
appkey_index=%u, scene_id=%u\r\n",
      pEvt->client_address,
      pEvt->appkey_index,
      pEvt->scene_id);
}

/***************************************************************************//**
 * Handling of scene setup server publish event. This is informational event.
 *
 * @param[in] pEvt  Pointer to scene setup server publish event.
 ******************************************************************************/
static void handle_scene_setup_server_publish_event(
  struct gecko_msg_mesh_scene_setup_server_publish_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scene_setup_server_publish_id, period_ms=%lu\r\n",
      pEvt->period_ms);
}

/*******************************************************************************
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
void handle_scenes_server_events(struct gecko_cmd_packet *pEvt)
{
  switch (BGLIB_MSG_ID(pEvt->header)) {
    case gecko_evt_mesh_scene_server_get_id:
      handle_scene_server_get_event(
        &(pEvt->data.evt_mesh_scene_server_get));
      break;

    case gecko_evt_mesh_scene_server_register_get_id:
      handle_scene_server_register_get_event(
        &(pEvt->data.evt_mesh_scene_server_register_get));
      break;

    case gecko_evt_mesh_scene_server_recall_id:
      handle_scene_server_recall_event(
        &(pEvt->data.evt_mesh_scene_server_recall));
      break;

    case gecko_evt_mesh_scene_server_publish_id:
      handle_scene_server_publish_event(
        &(pEvt->data.evt_mesh_scene_server_publish));
      break;

    case gecko_evt_mesh_scene_setup_server_store_id:
      handle_scene_setup_server_store_event(
        &(pEvt->data.evt_mesh_scene_setup_server_store));
      break;

    case gecko_evt_mesh_scene_setup_server_delete_id:
      handle_scene_setup_server_delete_event(
        &(pEvt->data.evt_mesh_scene_setup_server_delete));
      break;

    case gecko_evt_mesh_scene_setup_server_publish_id:
      handle_scene_setup_server_publish_event(
        &(pEvt->data.evt_mesh_scene_setup_server_publish));
      break;

    default:
      break;
  }
}

/** @} (end addtogroup Scenes) */
