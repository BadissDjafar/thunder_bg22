/***************************************************************************//**
 * @file  scheduler.c
 * @brief Scheduler module implementation
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
#include "scheduler.h"

/* C Standard Library headers */
#include <stdio.h>

#ifdef ENABLE_LOGGING
#define log(...) printf(__VA_ARGS__)
#else
#define log(...)
#endif

/***************************************************************************//**
 * @addtogroup Scheduler
 * @{
 ******************************************************************************/

/*******************************************************************************
 * Scheduler initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where scheduler model is initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t scheduler_init(uint16_t element)
{
  // Initialize scheduler server models
  uint16_t result = gecko_cmd_mesh_scheduler_server_init(element)->result;
  if (result) {
    log("mesh_scheduler_server_init failed, code 0x%x\r\n", result);
  }
  return result;
}

/***************************************************************************//**
 * Handling of scheduler server action changed event.
 *
 * @param[in] pEvt  Pointer to scheduler server action changed event.
 ******************************************************************************/
static void handle_scheduler_server_action_changed_event(
  struct gecko_msg_mesh_scheduler_server_action_changed_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_scheduler_server_action_changed_id, \
elem_index=%u, index=%u, ", pEvt->elem_index, pEvt->index);
  if (pEvt->year == 100) {
    log("year=Every, ");
  } else {
    log("year=%u, ", pEvt->year);
  }
  log("month=0x%03x, ", pEvt->month);
  if (pEvt->day) {
    log("day=0x%02x, ", pEvt->day);
  } else {
    log("day=Every, ");
  }
  if (pEvt->hour == 0x18) {
    log("hour=Every, ");
  } else if (pEvt->hour == 0x19) {
    log("hour=Random, ");
  } else {
    log("hour=%u, ", pEvt->hour);
  }
  if (pEvt->minute == 0x3c) {
    log("minute=Every, ");
  } else if (pEvt->minute == 0x3d) {
    log("minute=Every 15, ");
  } else if (pEvt->minute == 0x3e) {
    log("minute=Every 20, ");
  } else if (pEvt->minute == 0x3f) {
    log("minute=Random, ");
  } else {
    log("minute=%u, ", pEvt->minute);
  }
  if (pEvt->second == 0x3c) {
    log("second=Every, ");
  } else if (pEvt->second == 0x3d) {
    log("second=Every 15, ");
  } else if (pEvt->second == 0x3e) {
    log("second=Every 20, ");
  } else if (pEvt->second == 0x3f) {
    log("second=Random, ");
  } else {
    log("second=%u, ", pEvt->second);
  }
  log("day_of_week=0x%02x, ", pEvt->day_of_week);
  log("action=");
  switch (pEvt->action) {
    case 0x0:
      log("Turn Off");
      break;

    case 0x1:
      log("Turn On");
      break;

    case 0x2:
      log("Scene Recall");
      break;

    case 0xf:
      log("No action");
      break;

    default:
      break;
  }
  log(", transition_time=0x%02x, ", pEvt->transition_time);
  if (pEvt->scene_number) {
    log("scene_number=0x%04x", pEvt->scene_number);
  } else {
    log("scene_number=No scene");
  }
  log("\r\n");
}

/*******************************************************************************
 * Handling of mesh scheduler events.
 * It handles:
 *  - scheduler_server_action_changed
 *
 * @param[in] pEvt  Pointer to incoming scheduler event.
 ******************************************************************************/
void handle_scheduler_server_events(struct gecko_cmd_packet *pEvt)
{
  switch (BGLIB_MSG_ID(pEvt->header)) {
    case gecko_evt_mesh_scheduler_server_action_changed_id:
      handle_scheduler_server_action_changed_event(
        &(pEvt->data.evt_mesh_scheduler_server_action_changed));
      break;

    default:
      break;
  }
}

/** @} (end addtogroup Scheduler) */
