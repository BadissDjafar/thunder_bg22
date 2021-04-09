/***************************************************************************//**
 * @file  time.c
 * @brief Time module implementation
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

/* Own header */
#include "time.h"

/* C Standard Library headers */
#include <stdio.h>

#ifdef ENABLE_LOGGING
#define log(...) printf(__VA_ARGS__)
#else
#define log(...)
#endif

/***************************************************************************//**
 * @addtogroup Time
 * @{
 ******************************************************************************/

/*******************************************************************************
 * Time initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where time model is initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t time_init(uint16_t element)
{
  // Initialize time server models
  uint16_t result = gecko_cmd_mesh_time_server_init(element)->result;
  if (result) {
    log("mesh_time_server_init failed, code 0x%x\r\n", result);
  }
  return result;
}

/***************************************************************************//**
 * Handling of time server time updated event.
 *
 * @param[in] pEvt  Pointer to time server time updated event.
 ******************************************************************************/
static void handle_time_server_time_updated_event(
  struct gecko_msg_mesh_time_server_time_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_time_server_time_updated_id, tai_seconds=0x");
  // print only 40 bits as this is size of TAI Seconds
  log("%2.2x", (uint8_t)((pEvt->tai_seconds >> 32) & 0xFF));
  log("%4.4x", (uint16_t)((pEvt->tai_seconds >> 16) & 0xFFFF));
  log("%4.4x, ", (uint16_t)(pEvt->tai_seconds & 0xFFFF));
  log("subsecond=%u, uncertainty=%u ms, time_authority=%u, tai_utc_delta=%ld, \
time_zone_offset=%d\r\n",
      pEvt->subsecond,
      (uint16_t)pEvt->uncertainty * 10,
      pEvt->time_authority,
      pEvt->tai_utc_delta,
      pEvt->time_zone_offset);
}

/***************************************************************************//**
 * Handling of time server time zone offset updated event.
 *
 * @param[in] pEvt  Pointer to time server time zone offset updated event.
 ******************************************************************************/
static void handle_time_server_time_zone_offset_updated_event(
  struct gecko_msg_mesh_time_server_time_zone_offset_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_time_server_time_zone_offset_updated_id, \
zone_offset_current=%d, zone_offset_new=%d, tai_of_zone_change=0x",
      pEvt->time_zone_offset_current,
      pEvt->time_zone_offset_new);
  // print only 40 bits as this is size of TAI of Zone Change
  log("%2.2x", (uint8_t)((pEvt->tai_of_zone_change >> 32) & 0xFF));
  log("%4.4x", (uint16_t)((pEvt->tai_of_zone_change >> 16) & 0xFFFF));
  log("%4.4x", (uint16_t)(pEvt->tai_of_zone_change & 0xFFFF));
  log("\r\n");
}

/***************************************************************************//**
 * Handling of time server tai utc delta updated event.
 *
 * @param[in] pEvt  Pointer to time server tai utc delta updated event.
 ******************************************************************************/
static void handle_time_server_tai_utc_delta_updated_event(
  struct gecko_msg_mesh_time_server_tai_utc_delta_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_time_server_tai_utc_delta_updated_id, \
tai_utc_delta_current=%ld, tai_utc_delta_new=%ld, tai_of_delta_change=0x",
      pEvt->tai_utc_delta_current,
      pEvt->tai_utc_delta_new);
  // print only 40 bits as this is size of TAI of Delta Change
  log("%2.2x", (uint8_t)((pEvt->tai_of_delta_change >> 32) & 0xFF));
  log("%4.4x", (uint16_t)((pEvt->tai_of_delta_change >> 16) & 0xFFFF));
  log("%4.4x", (uint16_t)(pEvt->tai_of_delta_change & 0xFFFF));
  log("\r\n");
}

/***************************************************************************//**
 * Handling of time server time role updated event.
 *
 * @param[in] pEvt  Pointer to time server time role updated event.
 ******************************************************************************/
static void handle_time_server_time_role_updated_event(
  struct gecko_msg_mesh_time_server_time_role_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_time_server_time_role_updated_id, time_role=");
  switch (pEvt->time_role) {
    case mesh_time_client_time_role_none:
      log("None");
      break;

    case mesh_time_client_time_role_authority:
      log("Authority");
      break;

    case mesh_time_client_time_role_relay:
      log("Relay");
      break;

    case mesh_time_client_time_role_client:
      log("Client");
      break;

    default:
      break;
  }
  log("\r\n");
}

/*******************************************************************************
 * Handling of mesh time events.
 * It handles:
 *  - time_server_time_updated
 *  - time_server_time_zone_offset_updated
 *  - time_server_tai_utc_delta_updated
 *  - time_server_time_role_updated
 *
 * @param[in] pEvt  Pointer to incoming time event.
 ******************************************************************************/
void handle_time_server_events(struct gecko_cmd_packet *pEvt)
{
  switch (BGLIB_MSG_ID(pEvt->header)) {
    case gecko_evt_mesh_time_server_time_updated_id:
      handle_time_server_time_updated_event(
        &(pEvt->data.evt_mesh_time_server_time_updated));
      break;

    case gecko_evt_mesh_time_server_time_zone_offset_updated_id:
      handle_time_server_time_zone_offset_updated_event(
        &(pEvt->data.evt_mesh_time_server_time_zone_offset_updated));
      break;

    case gecko_evt_mesh_time_server_tai_utc_delta_updated_id:
      handle_time_server_tai_utc_delta_updated_event(
        &(pEvt->data.evt_mesh_time_server_tai_utc_delta_updated));
      break;

    case gecko_evt_mesh_time_server_time_role_updated_id:
      handle_time_server_time_role_updated_event(
        &(pEvt->data.evt_mesh_time_server_time_role_updated));
      break;

    default:
      break;
  }
}

/** @} (end addtogroup Time) */
