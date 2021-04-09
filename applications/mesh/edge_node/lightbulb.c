/***************************************************************************//**
 * @file
 * @brief Lightbulb module
 * This file implements a lightbulb with associated mesh models
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

#include "lightbulb.h"
#include "light_controller.h"
#include "scenes.h"
#include "time.h"
#include "scheduler.h"

/* C Standard Library headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Bluetooth stack headers */
#include "mesh_generic_model_capi_types.h"
#include "mesh_lighting_model_capi_types.h"
#include "mesh_lib.h"

/* LED driver with support for PWM dimming */
#include "led_driver.h"

#ifdef ENABLE_LOGGING
#define log(...) printf(__VA_ARGS__)
#else
#define log(...)
#endif

/***********************************************************************************************//**
 * @addtogroup Lightbulb
 * @{
 **************************************************************************************************/

/** Timer Frequency used. */
#define TIMER_CLK_FREQ ((uint32_t)32768)
/** Convert msec to timer ticks. Maximum valid parameter is 65535999 ms. */
#define TIMER_MS_2_TIMERTICK(ms) ((uint32_t)((TIMER_CLK_FREQ * (uint64_t)ms) / 1000))

/** Number of models handled by mesh_lib */
#define NUMBER_OF_MESH_LIB_MODELS         11
/** Immediate transition time is 0 seconds */
#define IMMEDIATE                          0
/** Values greater than max 37200000 are treated as unknown remaining time */
#define UNKNOWN_REMAINING_TIME      40000000

/*******************************************************************************
 * Timer handles defines.
 ******************************************************************************/
#define TIMER_ID_SAVE_STATE               60
#define TIMER_ID_ONOFF_TRANSITION         53
#define TIMER_ID_LIGHTNESS_TRANSITION     52
#define TIMER_ID_DELAYED_ONOFF            51
#define TIMER_ID_DELAYED_LIGHTNESS        50
#define TIMER_ID_DELAYED_PRI_LEVEL        49
#define TIMER_ID_PRI_LEVEL_TRANSITION     48
#define TIMER_ID_DELAYED_CTL              47
#define TIMER_ID_CTL_TRANSITION           46
#define TIMER_ID_DELAYED_CTL_TEMPERATURE  45
#define TIMER_ID_CTL_TEMP_TRANSITION      44
#define TIMER_ID_DELAYED_SEC_LEVEL        43
#define TIMER_ID_SEC_LEVEL_TRANSITION     42
#define TIMER_ID_PRI_LEVEL_MOVE           30
#define TIMER_ID_SEC_LEVEL_MOVE           29

static uint16_t _primary_elem_index = 0xffff; /* For indexing elements of the node */
static uint16_t _secondary_elem_index = 0xffff; /* For indexing elements of the node */

/***********************************************************************************************//**
 * \defgroup lightbulb_state Lightbulb State
 * \brief Manage lightbulb state.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup lightbulb_state
 * @{
 **************************************************************************************************/

/// Lightbulb state
static PACKSTRUCT(struct lightbulb_state {
  // On/Off Server state
  uint8_t onoff_current;          /**< Current generic on/off value */
  uint8_t onoff_target;           /**< Target generic on/off value */

  // Transition Time Server state
  uint8_t transtime;              /**< Transition time */

  // On Power Up Server state
  uint8_t onpowerup;              /**< On Power Up value */

  // Lightness server
  uint16_t lightness_current;     /**< Current lightness value */
  uint16_t lightness_target;      /**< Target lightness value */
  uint16_t lightness_last;        /**< Last lightness value */
  uint16_t lightness_default;     /**< Default lightness value */
  uint16_t lightness_min;         /**< Minimum lightness value */
  uint16_t lightness_max;         /**< Maximum lightness value */

  // Primary Generic Level
  int16_t pri_level_current;      /**< Current primary generic level value */
  int16_t pri_level_target;       /**< Target primary generic level value */

  // Temperature server
  uint16_t temperature_current;   /**< Current temperature value */
  uint16_t temperature_target;    /**< Target temperature value */
  uint16_t temperature_default;   /**< Default temperature value */
  uint16_t temperature_min;       /**< Minimum temperature value */
  uint16_t temperature_max;       /**< Maximum temperature value */

  // Delta UV
  int16_t deltauv_current;        /**< Current delta UV value */
  int16_t deltauv_target;         /**< Target delta UV value */
  int16_t deltauv_default;        /**< Default delta UV value */

  // Secondary Generic Level
  int16_t sec_level_current;      /**< Current secondary generic level value */
  int16_t sec_level_target;       /**< Target secondary generic level value */
}) lightbulb_state;

/** @} (end addtogroup lightbulb_state) */

/// copy of transition delay parameter, needed for delayed on/off request
static uint32_t delayed_onoff_trans = 0;
/// copy of transition delay parameter, needed for delayed lightness request
static uint32_t delayed_lightness_trans = 0;
/// copy of lightness request kind, needed for delayed lightness request
static mesh_generic_state_t lightness_kind = mesh_generic_state_last;
/// copy of transition delay parameter, needed for delayed primary generic level request
static uint32_t delayed_pri_level_trans = 0;
/// copy of generic request kind, needed for delayed primary generic request
static mesh_generic_request_t pri_level_request_kind = mesh_generic_request_level;
/// copy of move transition parameter for primary generic request
static uint32_t move_pri_level_trans = 0;
/// copy of move delta parameter for primary generic request
static int16_t move_pri_level_delta = 0;
/// copy of transition delay parameter, needed for delayed ctl request
static uint32_t delayed_ctl_trans = 0;
/// copy of transition delay parameter, needed for delayed temperature request
static uint32_t delayed_ctl_temperature_trans = 0;
/// copy of transition delay parameter, needed for delayed secondary generic level request
static uint32_t delayed_sec_level_trans = 0;
/// copy of generic request kind, needed for delayed secondary generic request
static mesh_generic_request_t sec_level_request_kind = mesh_generic_request_level;
/// copy of move transition parameter for secondary generic request
static uint32_t move_sec_level_trans = 0;
/// copy of move delta parameter for secondary generic request
static int16_t move_sec_level_delta = 0;

static void lightbulb_state_changed(void);
static errorcode_t ctl_temperature_update(uint16_t element_index, uint32_t remaining_ms);

/***********************************************************************************************//**
 * \defgroup mesh_models Mesh Models
 * \brief Mesh models associated with lightbulb.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup mesh_models
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * This function convert mesh format of default transition time to milliseconds.
 *
 * @return Default transition time in milliseconds.
 ******************************************************************************/
static uint32_t default_transition_time(void)
{
  return mesh_lib_transition_time_to_ms(lightbulb_state.transtime);
}

/***********************************************************************************************//**
 * \defgroup GenericOnOff
 * \brief Generic OnOff Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup GenericOnOff
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to generic on/off request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t onoff_response(uint16_t element_index,
                                  uint16_t client_addr,
                                  uint16_t appkey_index,
                                  uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_on_off;
  current.on_off.on = lightbulb_state.onoff_current;

  target.kind = mesh_generic_state_on_off;
  target.on_off.on = lightbulb_state.onoff_target;

  return mesh_lib_generic_server_response(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          &target,
                                          remaining_ms,
                                          0x00);
}

/***************************************************************************//**
 * Update generic on/off state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t onoff_update(uint16_t element_index, uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_on_off;
  current.on_off.on = lightbulb_state.onoff_current;

  target.kind = mesh_generic_state_on_off;
  target.on_off.on = lightbulb_state.onoff_target;

  return mesh_lib_generic_server_update(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update generic on/off state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t onoff_update_and_publish(uint16_t element_index,
                                            uint32_t remaining_ms)
{
  errorcode_t e;

  e = onoff_update(element_index, remaining_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_generic_state_on_off);
  }

  return e;
}

/***************************************************************************//**
 * This function process the requests for the generic on/off model.
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
static void onoff_request(uint16_t model_id,
                          uint16_t element_index,
                          uint16_t client_addr,
                          uint16_t server_addr,
                          uint16_t appkey_index,
                          const struct mesh_generic_request *request,
                          uint32_t transition_ms,
                          uint16_t delay_ms,
                          uint8_t request_flags)
{
  log("ON/OFF request: requested state=<%s>, transition=%lu, delay=%u\r\n",
      request->on_off ? "ON" : "OFF", transition_ms, delay_ms);

  if (lightbulb_state.onoff_current == request->on_off) {
    log("Request for current state received; no op\r\n");
  } else {
    log("Turning lightbulb <%s>\r\n", request->on_off ? "ON" : "OFF");
    if (transition_ms == 0 && delay_ms == 0) { // Immediate change
      lightbulb_state.onoff_current = request->on_off;
      lightbulb_state.onoff_target = request->on_off;
      if (lightbulb_state.onoff_current == MESH_GENERIC_ON_OFF_STATE_OFF) {
        lightbulb_state.lightness_target = 0;
      } else {
        // restore last brightness
        lightbulb_state.lightness_target = lightbulb_state.lightness_last;
      }
      LEDS_SetLevel(lightbulb_state.lightness_target, IMMEDIATE);
    } else if (delay_ms > 0) {
      // a delay has been specified for the light change. Start a soft timer
      // that will trigger the change after the given delay
      // Current state remains as is for now
      lightbulb_state.onoff_target = request->on_off;
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms), TIMER_ID_DELAYED_ONOFF, 1);
      // store transition parameter for later use
      delayed_onoff_trans = transition_ms;
    } else {
      // no delay but transition time has been set.
      lightbulb_state.onoff_target = request->on_off;
      if (lightbulb_state.onoff_target == MESH_GENERIC_ON_OFF_STATE_ON) {
        lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      }
      onoff_update(element_index, transition_ms);

      if (request->on_off == MESH_GENERIC_ON_OFF_STATE_OFF) {
        lightbulb_state.lightness_target = 0;
      } else {
        // restore last brightness
        lightbulb_state.lightness_target = lightbulb_state.lightness_last;
      }
      LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms), TIMER_ID_ONOFF_TRANSITION, 1);
    }
    lightbulb_state_changed();
    // State has changed, so the current scene number is reset
    gecko_cmd_mesh_scene_server_reset_register(element_index);
  }

  uint32_t remaining_ms = delay_ms + transition_ms;
  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    onoff_response(element_index, client_addr, appkey_index, remaining_ms);
  }
  onoff_update_and_publish(element_index, remaining_ms);
  // publish to bound states
  mesh_lib_generic_server_publish(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_lighting_state_lightness_actual);
}

/***************************************************************************//**
 * This function is a handler for generic on/off change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void onoff_change(uint16_t model_id,
                         uint16_t element_index,
                         const struct mesh_generic_state *current,
                         const struct mesh_generic_state *target,
                         uint32_t remaining_ms)
{
  if (current->on_off.on != lightbulb_state.onoff_current) {
    log("on-off state changed %u to %u\r\n", lightbulb_state.onoff_current, current->on_off.on);

    lightbulb_state.onoff_current = current->on_off.on;
    lightbulb_state_changed();
  } else {
    log("dummy onoff change - same state as before\r\n");
  }
}

/***************************************************************************//**
 * This function is a handler for generic on/off recall event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
static void onoff_recall(uint16_t model_id,
                         uint16_t element_index,
                         const struct mesh_generic_state *current,
                         const struct mesh_generic_state *target,
                         uint32_t transition_ms)
{
  log("Generic On/Off recall\r\n");
  if (transition_ms == IMMEDIATE) {
    lightbulb_state.onoff_target = current->on_off.on;
  } else {
    lightbulb_state.onoff_target = target->on_off.on;
  }

  if (lightbulb_state.onoff_current == lightbulb_state.onoff_target) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall ON/OFF state <%s> with transition=%lu ms\r\n",
        lightbulb_state.onoff_target ? "ON" : "OFF",
        transition_ms);

    if (transition_ms == IMMEDIATE) {
      lightbulb_state.onoff_current = current->on_off.on;
    } else {
      if (lightbulb_state.onoff_target == MESH_GENERIC_ON_OFF_STATE_ON) {
        lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      }
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_ONOFF_TRANSITION,
                                        1);
    }
    lightbulb_state_changed();
  }

  onoff_update_and_publish(element_index, transition_ms);
}

/***************************************************************************//**
 * This function is called when a light on/off request
 * with non-zero transition time has completed.
 ******************************************************************************/
static void onoff_transition_complete(void)
{
  // transition done -> set state, update and publish
  lightbulb_state.onoff_current = lightbulb_state.onoff_target;

  log("transition complete. New state is %s\r\n", lightbulb_state.onoff_current ? "ON" : "OFF");

  lightbulb_state_changed();
  onoff_update_and_publish(_primary_elem_index, IMMEDIATE);
}

/***************************************************************************//**
 * This function is called when delay for light on/off request has completed.
 ******************************************************************************/
static void delayed_onoff_request(void)
{
  log("starting delayed on/off request: %u -> %u, %lu ms\r\n",
      lightbulb_state.onoff_current,
      lightbulb_state.onoff_target,
      delayed_onoff_trans
      );

  if (delayed_onoff_trans == 0) {
    // no transition delay, update state immediately

    lightbulb_state.onoff_current = lightbulb_state.onoff_target;
    if (lightbulb_state.onoff_current == MESH_GENERIC_ON_OFF_STATE_OFF) {
      LEDS_SetState(LED_STATE_OFF);
    } else {
      // restore last brightness level
      LEDS_SetLevel(lightbulb_state.lightness_last, IMMEDIATE);
      lightbulb_state.lightness_current = lightbulb_state.lightness_last;
    }

    lightbulb_state_changed();
    onoff_update_and_publish(_primary_elem_index, delayed_onoff_trans);
  } else {
    if (lightbulb_state.onoff_target == MESH_GENERIC_ON_OFF_STATE_OFF) {
      lightbulb_state.lightness_target = 0;
    } else {
      // restore last brightness level, with transition delay
      lightbulb_state.lightness_target = lightbulb_state.lightness_last;
      lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      onoff_update(_primary_elem_index, delayed_onoff_trans);
    }
    LEDS_SetLevel(lightbulb_state.lightness_target, delayed_onoff_trans);

    // state is updated when transition is complete
    gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_onoff_trans), TIMER_ID_ONOFF_TRANSITION, 1);
  }
}

/** @} (end addtogroup GenericOnOff) */

/***********************************************************************************************//**
 * \defgroup GenericPowerOnOff
 * \brief Generic Power OnOff Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup GenericPowerOnOff
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to generic power on/off request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t power_onoff_response(uint16_t element_index,
                                        uint16_t client_addr,
                                        uint16_t appkey_index)
{
  struct mesh_generic_state current;
  current.kind = mesh_generic_state_on_power_up;
  current.on_power_up.on_power_up = lightbulb_state.onpowerup;

  return mesh_lib_generic_server_response(MESH_GENERIC_POWER_ON_OFF_SETUP_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          NULL,
                                          0,
                                          0x00);
}

/***************************************************************************//**
 * Update generic power on/off state.
 *
 * @param[in] element_index  Server model element index.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t power_onoff_update(uint16_t element_index)
{
  struct mesh_generic_state current;
  current.kind = mesh_generic_state_on_power_up;
  current.on_power_up.on_power_up = lightbulb_state.onpowerup;

  return mesh_lib_generic_server_update(MESH_GENERIC_POWER_ON_OFF_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        NULL,
                                        0);
}

/***************************************************************************//**
 * Update generic power on/off state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t power_onoff_update_and_publish(uint16_t element_index)
{
  errorcode_t e;

  e = power_onoff_update(element_index);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_GENERIC_POWER_ON_OFF_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_generic_state_on_power_up);
  }

  return e;
}

/***************************************************************************//**
 * This function process the requests for the generic power on/off model.
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
static void power_onoff_request(uint16_t model_id,
                                uint16_t element_index,
                                uint16_t client_addr,
                                uint16_t server_addr,
                                uint16_t appkey_index,
                                const struct mesh_generic_request *request,
                                uint32_t transition_ms,
                                uint16_t delay_ms,
                                uint8_t request_flags)
{
  log("ON POWER UP request received; state=<%s>\r\n",
      lightbulb_state.onpowerup == 0 ? "OFF"
      : lightbulb_state.onpowerup == 1 ? "ON"
      : "RESTORE");

  if (lightbulb_state.onpowerup == request->on_power_up) {
    log("Request for current state received; no op\r\n");
  } else {
    log("Setting onpowerup to <%s>\r\n",
        request->on_power_up == 0 ? "OFF"
        : request->on_power_up == 1 ? "ON"
        : "RESTORE");
    lightbulb_state.onpowerup = request->on_power_up;
    lightbulb_state_changed();
  }

  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    power_onoff_response(element_index, client_addr, appkey_index);
  }
  power_onoff_update_and_publish(element_index);
}

/***************************************************************************//**
 * This function is a handler for generic power on/off change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void power_onoff_change(uint16_t model_id,
                               uint16_t element_index,
                               const struct mesh_generic_state *current,
                               const struct mesh_generic_state *target,
                               uint32_t remaining_ms)
{
  // TODO
}

/** @} (end addtogroup GenericPowerOnOff) */

/***********************************************************************************************//**
 * \defgroup GenericTransitionTime
 * \brief Generic Default Transition Time Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup GenericTransitionTime
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to generic default transition time request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t transtime_response(uint16_t element_index,
                                      uint16_t client_addr,
                                      uint16_t appkey_index)
{
  struct mesh_generic_state current;
  current.kind = mesh_generic_state_transition_time;
  current.transition_time.time = lightbulb_state.transtime;

  return mesh_lib_generic_server_response(MESH_GENERIC_TRANSITION_TIME_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          NULL,
                                          0,
                                          0x00);
}

/***************************************************************************//**
 * Update generic default transition time state.
 *
 * @param[in] element_index  Server model element index.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t transtime_update(uint16_t element_index)
{
  struct mesh_generic_state current;
  current.kind = mesh_generic_state_transition_time;
  current.transition_time.time = lightbulb_state.transtime;

  return mesh_lib_generic_server_update(MESH_GENERIC_TRANSITION_TIME_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        NULL,
                                        0);
}

/***************************************************************************//**
 * Update generic default transition time state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t transtime_update_and_publish(uint16_t element_index)
{
  errorcode_t e;

  e = transtime_update(element_index);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_GENERIC_TRANSITION_TIME_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_generic_state_transition_time);
  }

  return e;
}

/***************************************************************************//**
 * This function process the requests for the generic default transition time
 * model.
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
static void transtime_request(uint16_t model_id,
                              uint16_t element_index,
                              uint16_t client_addr,
                              uint16_t server_addr,
                              uint16_t appkey_index,
                              const struct mesh_generic_request *request,
                              uint32_t transition_ms,
                              uint16_t delay_ms,
                              uint8_t request_flags)
{
  log("TRANSTIME request received; state=<0x%x>\r\n",
      lightbulb_state.transtime);

  if (lightbulb_state.transtime == request->transition_time) {
    log("Request for current state received; no op\r\n");
  } else {
    log("Setting transtime to <0x%x>\r\n", request->transition_time);
    lightbulb_state.transtime = request->transition_time;
    lightbulb_state_changed();
  }

  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    transtime_response(element_index, client_addr, appkey_index);
  }
  transtime_update_and_publish(element_index);
}

/***************************************************************************//**
 * This function is a handler for generic default transition time change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void transtime_change(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t remaining_ms)
{
  // TODO
}

/** @} (end addtogroup GenericTransitionTime) */

/***********************************************************************************************//**
 * \defgroup LightLightness
 * \brief Light Lightness Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup LightLightness
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Convert from lightness actual to lightness linear value.
 *
 * @param[in] actual  Actual value that is converted.
 *
 * @return Linear value.
 ******************************************************************************/
static uint16_t actual2linear(uint16_t actual)
{
  uint32_t linear = ((uint32_t)actual * actual + 65534) / 65535;
  return (uint16_t)linear;
}

/***************************************************************************//**
 * Convert from lightness linear to lightness actual value.
 *
 * @param[in] linear  Linear value that is converted.
 *
 * @return Actual value.
 ******************************************************************************/
static uint16_t linear2actual(uint16_t linear)
{
  uint32_t actual = (uint32_t)sqrt(65535 * (uint32_t)linear);
  return (uint16_t)actual;
}

/***************************************************************************//**
 * Response to light lightness request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 * @param[in] kind           Type of state used in light lightness response.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lightness_response(uint16_t element_index,
                                      uint16_t client_addr,
                                      uint16_t appkey_index,
                                      uint32_t remaining_ms,
                                      mesh_generic_state_t kind)
{
  struct mesh_generic_state current, target;

  current.kind = kind;
  if (kind == mesh_lighting_state_lightness_actual) {
    current.lightness.level = lightbulb_state.lightness_current;
  } else {
    current.lightness.level = actual2linear(lightbulb_state.lightness_current);
  }

  target.kind = kind;
  if (kind == mesh_lighting_state_lightness_actual) {
    target.lightness.level = lightbulb_state.lightness_target;
  } else {
    target.lightness.level = actual2linear(lightbulb_state.lightness_target);
  }

  return mesh_lib_generic_server_response(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          &target,
                                          remaining_ms,
                                          0x00);
}

/***************************************************************************//**
 * Update light lightness state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 * @param[in] kind           Type of state used in light lightness update.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lightness_update(uint16_t element_index,
                                    uint32_t remaining_ms,
                                    mesh_generic_state_t kind)
{
  struct mesh_generic_state current, target;

  current.kind = kind;
  if (kind == mesh_lighting_state_lightness_actual) {
    current.lightness.level = lightbulb_state.lightness_current;
  } else {
    current.lightness.level = actual2linear(lightbulb_state.lightness_current);
  }

  target.kind = kind;
  if (kind == mesh_lighting_state_lightness_actual) {
    target.lightness.level = lightbulb_state.lightness_target;
  } else {
    target.lightness.level = actual2linear(lightbulb_state.lightness_target);
  }

  return mesh_lib_generic_server_update(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update light lightness state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 * @param[in] kind           Type of state used in light lightness update and publish.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lightness_update_and_publish(uint16_t element_index,
                                                uint32_t remaining_ms,
                                                mesh_generic_state_t kind)
{
  errorcode_t e;

  e = lightness_update(element_index, remaining_ms, kind);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                        element_index,
                                        kind);
  }

  return e;
}

/***************************************************************************//**
 * This function process the requests for the light lightness model.
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
static void lightness_request(uint16_t model_id,
                              uint16_t element_index,
                              uint16_t client_addr,
                              uint16_t server_addr,
                              uint16_t appkey_index,
                              const struct mesh_generic_request *request,
                              uint32_t transition_ms,
                              uint16_t delay_ms,
                              uint8_t request_flags)
{
  uint16_t actual_request = 0;

  switch (request->kind) {
    case mesh_lighting_request_lightness_actual:
      lightness_kind = mesh_lighting_state_lightness_actual;
      actual_request = request->lightness;
      break;

    case mesh_lighting_request_lightness_linear:
      lightness_kind = mesh_lighting_state_lightness_linear;
      actual_request = linear2actual(request->lightness);
      break;

    default:
      break;
  }

  log("lightness_request: level=%u, transition=%lu, delay=%u\r\n",
      actual_request, transition_ms, delay_ms);

  if (lightbulb_state.lightness_current == actual_request) {
    log("Request for current state received; no op\r\n");
  } else {
    log("Setting lightness to <%u>\r\n", actual_request);
    if (transition_ms == 0 && delay_ms == 0) { // Immediate change
      lightbulb_state.lightness_current = actual_request;
      lightbulb_state.lightness_target = actual_request;
      if (actual_request != 0) {
        lightbulb_state.lightness_last = actual_request;
      }

      // update LED PWM duty cycle
      LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
    } else if (delay_ms > 0) {
      // a delay has been specified for the light change. Start a soft timer
      // that will trigger the change after the given delay
      // Current state remains as is for now
      lightbulb_state.lightness_target = actual_request;
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms), TIMER_ID_DELAYED_LIGHTNESS, 1);
      // store transition parameter for later use
      delayed_lightness_trans = transition_ms;
    } else {
      // no delay but transition time has been set.
      lightbulb_state.lightness_target = actual_request;
      LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);

      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms), TIMER_ID_LIGHTNESS_TRANSITION, 1);
    }
    lightbulb_state_changed();
    // State has changed, so the current scene number is reset
    gecko_cmd_mesh_scene_server_reset_register(element_index);
  }

  uint32_t remaining_ms = delay_ms + transition_ms;
  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    lightness_response(element_index, client_addr, appkey_index, remaining_ms, lightness_kind);
  }
  lightness_update_and_publish(element_index, remaining_ms, lightness_kind);
  // publish to bound states
  if (lightness_kind == mesh_lighting_state_lightness_actual) {
    mesh_lib_generic_server_publish(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                    element_index,
                                    mesh_lighting_state_lightness_linear);
  } else {
    mesh_lib_generic_server_publish(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                    element_index,
                                    mesh_lighting_state_lightness_actual);
  }
  mesh_lib_generic_server_publish(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_generic_state_on_off);
  mesh_lib_generic_server_publish(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_generic_state_level);
  mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_lighting_state_ctl);
}

/***************************************************************************//**
 * This function is a handler for light lightness change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void lightness_change(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t remaining_ms)
{
  if (current->kind != mesh_lighting_state_lightness_actual) {
    // if kind is not 'actual' then just report the change here, no change to light state
    log("lightness change, kind %u, value %u\r\n", current->kind, current->lightness.level);
    return;
  }

  if (lightbulb_state.lightness_current != current->lightness.level) {
    log("lightness_change: from %u to %u\r\n", lightbulb_state.lightness_current, current->lightness.level);
    lightbulb_state.lightness_current = current->lightness.level;
    lightbulb_state_changed();
  } else {
    log("lightness update -same value (%d)\r\n", lightbulb_state.lightness_current);
  }
}

/***************************************************************************//**
 * This function is a handler for light lightness recall event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
static void lightness_recall(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t transition_ms)
{
  log("Light Lightness recall\r\n");
  if (current->kind != mesh_lighting_state_lightness_actual) {
    return;
  }

  if (transition_ms == IMMEDIATE) {
    lightbulb_state.lightness_target = current->lightness.level;
  } else {
    lightbulb_state.lightness_target = target->lightness.level;
  }

  if (lightbulb_state.lightness_current == lightbulb_state.lightness_target) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall lightness to %u with transition=%lu ms\r\n",
        lightbulb_state.lightness_target,
        transition_ms);
    LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);

    if (transition_ms == IMMEDIATE) {
      lightbulb_state.lightness_current = current->lightness.level;
    } else {
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_LIGHTNESS_TRANSITION,
                                        1);
    }
    lightbulb_state_changed();
  }

  lightness_update_and_publish(element_index,
                               transition_ms,
                               mesh_lighting_state_lightness_actual);
}

/***************************************************************************//**
 * This function is called when a light lightness request
 * with non-zero transition time has completed.
 ******************************************************************************/
static void lightness_transition_complete(void)
{
  // transition done -> set state, update and publish
  lightbulb_state.lightness_current = lightbulb_state.lightness_target;
  if (lightbulb_state.lightness_target != 0) {
    lightbulb_state.lightness_last = lightbulb_state.lightness_target;
  }

  log("transition complete. New level is %u\r\n", lightbulb_state.lightness_current);

  lightbulb_state_changed();
  lightness_update_and_publish(_primary_elem_index, IMMEDIATE, lightness_kind);
}

/***************************************************************************//**
 * This function is called when delay for light lightness request has completed.
 ******************************************************************************/
static void delayed_lightness_request(void)
{
  log("starting delayed lightness request: level %u -> %u, %lu ms\r\n",
      lightbulb_state.lightness_current,
      lightbulb_state.lightness_target,
      delayed_lightness_trans
      );

  LEDS_SetLevel(lightbulb_state.lightness_target, delayed_lightness_trans);

  if (delayed_lightness_trans == 0) {
    // no transition delay, update state immediately
    lightbulb_state.lightness_current = lightbulb_state.lightness_target;
    if (lightbulb_state.lightness_target != 0) {
      lightbulb_state.lightness_last = lightbulb_state.lightness_target;
    }

    lightbulb_state_changed();
    lightness_update_and_publish(_primary_elem_index, delayed_lightness_trans, lightness_kind);
  } else {
    // state is updated when transition is complete
    gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_lightness_trans), TIMER_ID_LIGHTNESS_TRANSITION, 1);
  }
}

/** @} (end addtogroup LightLightness) */

/***********************************************************************************************//**
 * \defgroup LightLightnessSetup
 * \brief Light Lightness Setup Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup LightLightnessSetup
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to light lightness setup request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] kind           Type of state used in light lightness setup response.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lightness_setup_response(uint16_t element_index,
                                            uint16_t client_addr,
                                            uint16_t appkey_index,
                                            mesh_generic_state_t kind)
{
  struct mesh_generic_state current;

  current.kind = kind;

  switch (kind) {
    case mesh_lighting_state_lightness_default:
      current.lightness.level = lightbulb_state.lightness_default;
      break;

    case mesh_lighting_state_lightness_range:
      current.lightness_range.min = lightbulb_state.lightness_min;
      current.lightness_range.max = lightbulb_state.lightness_max;
      break;

    default:
      break;
  }

  return mesh_lib_generic_server_response(MESH_LIGHTING_LIGHTNESS_SETUP_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          NULL,
                                          0,
                                          0x00);
}

/***************************************************************************//**
 * Update light lightness setup state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] kind           Type of state used in light lightness setup update.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lightness_setup_update(uint16_t element_index, mesh_generic_state_t kind)
{
  struct mesh_generic_state current;

  current.kind = kind;

  switch (kind) {
    case mesh_lighting_state_lightness_default:
      current.lightness.level = lightbulb_state.lightness_default;
      break;

    case mesh_lighting_state_lightness_range:
      current.lightness_range.min = lightbulb_state.lightness_min;
      current.lightness_range.max = lightbulb_state.lightness_max;
      break;

    default:
      break;
  }

  return mesh_lib_generic_server_update(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        NULL,
                                        0);
}

/***************************************************************************//**
 * This function process the requests for the light lightness setup model.
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
static void lightness_setup_request(uint16_t model_id,
                                    uint16_t element_index,
                                    uint16_t client_addr,
                                    uint16_t server_addr,
                                    uint16_t appkey_index,
                                    const struct mesh_generic_request *request,
                                    uint32_t transition_ms,
                                    uint16_t delay_ms,
                                    uint8_t request_flags)
{
  mesh_generic_state_t kind = mesh_generic_state_last;
  switch (request->kind) {
    case mesh_lighting_request_lightness_default:
      kind = mesh_lighting_state_lightness_default;
      log("lightness_setup_request: state=lightness_default, default_lightness=%u\r\n",
          request->lightness);

      if (lightbulb_state.lightness_default == request->lightness) {
        log("Request for current state received; no op\r\n");
      } else {
        log("Setting default lightness to <%u>\r\n", request->lightness);
        lightbulb_state.lightness_default = request->lightness;
        lightbulb_state_changed();
      }
      break;

    case mesh_lighting_request_lightness_range:
      kind = mesh_lighting_state_lightness_range;
      log("lightness_setup_request: state=lightness_range, min_lightness=%u, max_lightness=%u\r\n",
          request->lightness_range.min, request->lightness_range.max);

      if ((lightbulb_state.lightness_min == request->lightness_range.min)
          && (lightbulb_state.lightness_max == request->lightness_range.max)) {
        log("Request for current state received; no op\r\n");
      } else {
        if (lightbulb_state.lightness_min != request->lightness_range.min) {
          log("Setting min lightness to <%u>\r\n", request->lightness_range.min);
          lightbulb_state.lightness_min = request->lightness_range.min;
          if (lightbulb_state.lightness_current < request->lightness_range.min
              && lightbulb_state.lightness_current != 0) {
            lightbulb_state.lightness_current = request->lightness_range.min;
            LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
          }
        }
        if (lightbulb_state.lightness_max != request->lightness_range.max) {
          log("Setting max lightness to <%u>\r\n", request->lightness_range.max);
          lightbulb_state.lightness_max = request->lightness_range.max;
          if (lightbulb_state.lightness_current > request->lightness_range.max) {
            lightbulb_state.lightness_current = request->lightness_range.max;
            LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
          }
        }
        lightbulb_state_changed();
      }
      break;

    default:
      break;
  }

  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    lightness_setup_response(element_index, client_addr, appkey_index, kind);
  } else {
    lightness_setup_update(element_index, kind);
  }
}

/***************************************************************************//**
 * This function is a handler for light lightness setup change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void lightness_setup_change(uint16_t model_id,
                                   uint16_t element_index,
                                   const struct mesh_generic_state *current,
                                   const struct mesh_generic_state *target,
                                   uint32_t remaining_ms)
{
  switch (current->kind) {
    case mesh_lighting_state_lightness_default:
      if (lightbulb_state.lightness_default != current->lightness.level) {
        log("default_lightness_change: from %u to %u\r\n", lightbulb_state.lightness_default, current->lightness.level);
        lightbulb_state.lightness_default = current->lightness.level;
        lightbulb_state_changed();
      } else {
        log("default lightness update -same value (%u)\r\n", lightbulb_state.lightness_default);
      }
      break;

    case mesh_lighting_state_lightness_range:
      if (lightbulb_state.lightness_min != current->lightness_range.min) {
        log("min_lightness_change: from %u to %u\r\n", lightbulb_state.lightness_min, current->lightness_range.min);
        lightbulb_state.lightness_min = current->lightness_range.min;
        lightbulb_state_changed();
      } else {
        log("min lightness update -same value (%u)\r\n", lightbulb_state.lightness_min);
      }

      if (lightbulb_state.lightness_max != current->lightness_range.max) {
        log("max_lightness_change: from %u to %u\r\n", lightbulb_state.lightness_max, current->lightness_range.max);
        lightbulb_state.lightness_max = current->lightness_range.max;
        lightbulb_state_changed();
      } else {
        log("max lightness update -same value (%u)\r\n", lightbulb_state.lightness_max);
      }

      break;

    default:
      break;
  }
}

/** @} (end addtogroup LightLightnessSetup) */

/***********************************************************************************************//**
 * \defgroup PriGenericLevel
 * \brief Generic Level Server model on primary element.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup PriGenericLevel
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to generic level request on primary element.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t pri_level_response(uint16_t element_index,
                                      uint16_t client_addr,
                                      uint16_t appkey_index,
                                      uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_level;
  current.level.level = lightbulb_state.pri_level_current;

  target.kind = mesh_generic_state_level;
  target.level.level = lightbulb_state.pri_level_target;

  return mesh_lib_generic_server_response(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          &target,
                                          remaining_ms,
                                          0x00);
}

/***************************************************************************//**
 * Update generic level state on primary element.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t pri_level_update(uint16_t element_index,
                                    uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_level;
  current.level.level = lightbulb_state.pri_level_current;

  target.kind = mesh_generic_state_level;
  target.level.level = lightbulb_state.pri_level_target;

  return mesh_lib_generic_server_update(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update generic level state on primary element
 * and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t pri_level_update_and_publish(uint16_t element_index,
                                                uint32_t remaining_ms)
{
  errorcode_t e;

  e = pri_level_update(element_index, remaining_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_generic_state_level);
  }

  return e;
}

/***************************************************************************//**
 * Schedule next generic level move request on primary element.
 *
 * @param[in] remaining_delta   The remaining level delta to the target state.
 ******************************************************************************/
static void pri_level_move_schedule_next_request(int32_t remaining_delta)
{
  uint32_t transition_ms = 0;
  if (abs(remaining_delta) < abs(move_pri_level_delta)) {
    transition_ms = (uint32_t)(((int64_t)move_pri_level_trans * remaining_delta)
                               / move_pri_level_delta);
    LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);
  } else {
    transition_ms = move_pri_level_trans;
    LEDS_SetLevel(lightbulb_state.lightness_current + move_pri_level_delta,
                  move_pri_level_trans);
  }
  gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                    TIMER_ID_PRI_LEVEL_MOVE,
                                    1);
}

/***************************************************************************//**
 * Handle generic level move request on primary element.
 ******************************************************************************/
static void pri_level_move_request(void)
{
  log("primary level move: level %d -> %d, delta %d in %lu ms\r\n",
      lightbulb_state.pri_level_current,
      lightbulb_state.pri_level_target,
      move_pri_level_delta,
      move_pri_level_trans);

  int32_t remaining_delta = (int32_t)lightbulb_state.pri_level_target
                            - lightbulb_state.pri_level_current;

  if (abs(remaining_delta) < abs(move_pri_level_delta)) {
    // end of move level as it reached target state
    lightbulb_state.pri_level_current = lightbulb_state.pri_level_target;
    lightbulb_state.lightness_current = lightbulb_state.lightness_target;
  } else {
    lightbulb_state.pri_level_current += move_pri_level_delta;
    lightbulb_state.lightness_current += move_pri_level_delta;
  }
  lightbulb_state_changed();
  pri_level_update_and_publish(_primary_elem_index, UNKNOWN_REMAINING_TIME);

  remaining_delta = (int32_t)lightbulb_state.pri_level_target
                    - lightbulb_state.pri_level_current;
  if (remaining_delta != 0) {
    pri_level_move_schedule_next_request(remaining_delta);
  }
}

/***************************************************************************//**
 * Stop generic level move on primary element.
 ******************************************************************************/
static void pri_level_move_stop(void)
{
  // Cancel timers
  gecko_cmd_hardware_set_soft_timer(0, TIMER_ID_DELAYED_PRI_LEVEL, 1);
  gecko_cmd_hardware_set_soft_timer(0, TIMER_ID_PRI_LEVEL_MOVE, 1);
  //Reset move parameters
  move_pri_level_delta = 0;
  move_pri_level_trans = 0;
}

/***************************************************************************//**
 * This function process the requests for the generic level model
 * on primary element.
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
static void pri_level_request(uint16_t model_id,
                              uint16_t element_index,
                              uint16_t client_addr,
                              uint16_t server_addr,
                              uint16_t appkey_index,
                              const struct mesh_generic_request *request,
                              uint32_t transition_ms,
                              uint16_t delay_ms,
                              uint8_t request_flags)
{
  uint16_t lightness;
  uint32_t remaining_ms = UNKNOWN_REMAINING_TIME;

  switch (request->kind) {
    case mesh_generic_request_level:
      log("pri_level_request: level=%d, transition=%lu, delay=%u\r\n",
          request->level, transition_ms, delay_ms);

      pri_level_move_stop();
      if (lightbulb_state.pri_level_current == request->level) {
        log("Request for current state received; no op\r\n");
        lightbulb_state.pri_level_target = request->level;
      } else {
        log("Setting pri_level to <%d>\r\n", request->level);

        lightness = request->level + 32768;

        if (transition_ms == 0 && delay_ms == 0) { // Immediate change
          lightbulb_state.pri_level_current = request->level;
          lightbulb_state.pri_level_target = request->level;
          lightbulb_state.lightness_current = lightness;
          lightbulb_state.lightness_target = lightness;

          // update LED Level
          LEDS_SetLevel(lightness, IMMEDIATE);
        } else if (delay_ms > 0) {
          // a delay has been specified for the change. Start a soft timer
          // that will trigger the change after the given delay
          // Current state remains as is for now
          lightbulb_state.pri_level_target = request->level;
          lightbulb_state.lightness_target = lightness;
          pri_level_request_kind = mesh_generic_request_level;
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                            TIMER_ID_DELAYED_PRI_LEVEL,
                                            1);
          // store transition parameter for later use
          delayed_pri_level_trans = transition_ms;
        } else {
          // no delay but transition time has been set.
          lightbulb_state.pri_level_target = request->level;
          lightbulb_state.lightness_target = lightness;
          LEDS_SetLevel(lightness, transition_ms);

          // lightbulb current state will be updated when transition is complete
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                            TIMER_ID_PRI_LEVEL_TRANSITION,
                                            1);
        }
        // State has changed, so the current scene number is reset
        gecko_cmd_mesh_scene_server_reset_register(element_index);
      }

      remaining_ms = delay_ms + transition_ms;
      break;

    case mesh_generic_request_level_move:
      log("pri_level_move_request: delta=%d, transition=%lu, delay=%u\r\n",
          request->level, transition_ms, delay_ms);
      // Store move parameters
      move_pri_level_delta = request->level;
      move_pri_level_trans = transition_ms;

      int16_t requested_level = 0;
      if (move_pri_level_delta > 0) {
        requested_level = 0x7FFF; // Max level value
      } else if (move_pri_level_delta < 0) {
        requested_level = 0x8000; // Min level value
      }

      if (lightbulb_state.pri_level_current == requested_level) {
        log("Request for current state received; no op\r\n");
        lightbulb_state.pri_level_target = requested_level;
        remaining_ms = IMMEDIATE;
      } else {
        log("Setting pri_level to <%d>\r\n", requested_level);

        lightness = requested_level + 32768;

        if (delay_ms > 0) {
          // a delay has been specified for the move. Start a soft timer
          // that will trigger the move after the given delay
          // Current state remains as is for now
          lightbulb_state.pri_level_target = requested_level;
          lightbulb_state.lightness_target = lightness;
          pri_level_request_kind = mesh_generic_request_level_move;
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                            TIMER_ID_DELAYED_PRI_LEVEL,
                                            1);
        } else {
          // no delay so start move
          lightbulb_state.pri_level_target = requested_level;
          lightbulb_state.lightness_target = lightness;

          int32_t remaining_delta = (int32_t)lightbulb_state.pri_level_target
                                    - lightbulb_state.pri_level_current;
          pri_level_move_schedule_next_request(remaining_delta);
        }
        remaining_ms = UNKNOWN_REMAINING_TIME;
        // State has changed, so the current scene number is reset
        gecko_cmd_mesh_scene_server_reset_register(element_index);
      }
      break;

    case mesh_generic_request_level_halt:
      log("pri_level_halt_request\r\n");

      // Set current state
      lightbulb_state.lightness_current = LEDS_GetLevel();
      lightbulb_state.lightness_target = lightbulb_state.lightness_current;
      lightbulb_state.pri_level_current = lightbulb_state.lightness_current - 32768;
      lightbulb_state.pri_level_target = lightbulb_state.pri_level_current;
      if (delay_ms > 0) {
        // a delay has been specified for the move halt. Start a soft timer
        // that will trigger the move halt after the given delay
        // Current state remains as is for now
        remaining_ms = delay_ms;
        pri_level_request_kind = mesh_generic_request_level_halt;
        gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                          TIMER_ID_DELAYED_PRI_LEVEL,
                                          1);
      } else {
        pri_level_move_stop();
        LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
        remaining_ms = IMMEDIATE;
      }
      break;

    default:
      break;
  }

  lightbulb_state_changed();

  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    pri_level_response(element_index, client_addr, appkey_index, remaining_ms);
  }
  pri_level_update_and_publish(element_index, remaining_ms);
  // publish to bound states
  mesh_lib_generic_server_publish(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_lighting_state_lightness_actual);
}

/***************************************************************************//**
 * This function is a handler for generic level change event
 * on primary element.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void pri_level_change(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t remaining_ms)
{
  if (lightbulb_state.pri_level_current != current->level.level) {
    log("pri_level_change: from %d to %d\r\n",
        lightbulb_state.pri_level_current,
        current->level.level);
    lightbulb_state.pri_level_current = current->level.level;
    lightbulb_state_changed();
    pri_level_move_stop();
  } else {
    log("pri_level update -same value (%d)\r\n",
        lightbulb_state.pri_level_current);
  }
}

/***************************************************************************//**
 * This function is a handler for generic level recall event on primary element.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
static void pri_level_recall(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t transition_ms)
{
  log("Primary Generic Level recall\r\n");
  if (transition_ms == IMMEDIATE) {
    lightbulb_state.pri_level_target = current->level.level;
  } else {
    lightbulb_state.pri_level_target = target->level.level;
  }

  if (lightbulb_state.pri_level_current == lightbulb_state.pri_level_target) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall pri_level to %d with transition=%lu ms\r\n",
        lightbulb_state.pri_level_target,
        transition_ms);
    if (transition_ms == IMMEDIATE) {
      lightbulb_state.pri_level_current = current->level.level;
    } else {
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_PRI_LEVEL_TRANSITION,
                                        1);
    }
    lightbulb_state_changed();
  }

  pri_level_update_and_publish(element_index, transition_ms);
}

/***************************************************************************//**
 * This function is called when a generic level request on primary element
 * with non-zero transition time has completed.
 ******************************************************************************/
static void pri_level_transition_complete(void)
{
  // transition done -> set state, update and publish
  lightbulb_state.pri_level_current = lightbulb_state.pri_level_target;
  lightbulb_state.lightness_current = lightbulb_state.lightness_target;

  log("transition complete. New pri_level is %d\r\n",
      lightbulb_state.pri_level_current);

  lightbulb_state_changed();
  pri_level_update_and_publish(_primary_elem_index, IMMEDIATE);
}

/***************************************************************************//**
 * This function is called when delay for generic level request
 * on primary element has completed.
 ******************************************************************************/
static void delayed_pri_level_request(void)
{
  log("starting delayed primary level request: level %d -> %d, %lu ms\r\n",
      lightbulb_state.pri_level_current,
      lightbulb_state.pri_level_target,
      delayed_pri_level_trans);

  switch (pri_level_request_kind) {
    case mesh_generic_request_level:
      LEDS_SetLevel(lightbulb_state.lightness_target, delayed_pri_level_trans);

      if (delayed_pri_level_trans == 0) {
        // no transition delay, update state immediately
        lightbulb_state.pri_level_current = lightbulb_state.pri_level_target;
        lightbulb_state.lightness_current = lightbulb_state.lightness_target;

        lightbulb_state_changed();
        pri_level_update_and_publish(_primary_elem_index, delayed_pri_level_trans);
      } else {
        // state is updated when transition is complete
        gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_pri_level_trans),
                                          TIMER_ID_PRI_LEVEL_TRANSITION,
                                          1);
      }
      break;

    case mesh_generic_request_level_move:
      pri_level_move_schedule_next_request((int32_t)lightbulb_state.pri_level_target
                                           - lightbulb_state.pri_level_current);
      pri_level_update_and_publish(_primary_elem_index, UNKNOWN_REMAINING_TIME);
      break;

    case mesh_generic_request_level_halt:
      // Set current state
      lightbulb_state.lightness_current = LEDS_GetLevel();
      lightbulb_state.lightness_target = lightbulb_state.lightness_current;
      lightbulb_state.pri_level_current = lightbulb_state.lightness_current - 32768;
      lightbulb_state.pri_level_target = lightbulb_state.pri_level_current;
      pri_level_move_stop();
      LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
      pri_level_update_and_publish(_primary_elem_index, IMMEDIATE);
      break;

    default:
      break;
  }
}

/** @} (end addtogroup PriGenericLevel) */

/***********************************************************************************************//**
 * \defgroup LightCTL
 * \brief Light CTL Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup LightCTL
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to light CTL request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_response(uint16_t element_index,
                                uint16_t client_addr,
                                uint16_t appkey_index,
                                uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_lighting_state_ctl;
  current.ctl.lightness = lightbulb_state.lightness_current;
  current.ctl.temperature = lightbulb_state.temperature_current;
  current.ctl.deltauv = lightbulb_state.deltauv_current;

  target.kind = mesh_lighting_state_ctl;
  target.ctl.lightness = lightbulb_state.lightness_target;
  target.ctl.temperature = lightbulb_state.temperature_target;
  target.ctl.deltauv = lightbulb_state.deltauv_target;

  return mesh_lib_generic_server_response(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          &target,
                                          remaining_ms,
                                          0x00);
}

/***************************************************************************//**
 * Update light CTL state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_update(uint16_t element_index, uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_lighting_state_ctl;
  current.ctl.lightness = lightbulb_state.lightness_current;
  current.ctl.temperature = lightbulb_state.temperature_current;
  current.ctl.deltauv = lightbulb_state.deltauv_current;

  target.kind = mesh_lighting_state_ctl;
  target.ctl.lightness = lightbulb_state.lightness_target;
  target.ctl.temperature = lightbulb_state.temperature_target;
  target.ctl.deltauv = lightbulb_state.deltauv_target;

  return mesh_lib_generic_server_update(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update light CTL state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_update_and_publish(uint16_t element_index,
                                          uint32_t remaining_ms)
{
  errorcode_t e;

  e = ctl_update(element_index, remaining_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_lighting_state_ctl);
  }

  return e;
}

/***************************************************************************//**
 * This function process the requests for the light CTL model.
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
static void ctl_request(uint16_t model_id,
                        uint16_t element_index,
                        uint16_t client_addr,
                        uint16_t server_addr,
                        uint16_t appkey_index,
                        const struct mesh_generic_request *request,
                        uint32_t transition_ms,
                        uint16_t delay_ms,
                        uint8_t request_flags)
{
  log("ctl_request: lightness=%u, temperature=%u, delta_uv=%d, transition=%lu, delay=%u\r\n",
      request->ctl.lightness, request->ctl.temperature, request->ctl.deltauv, transition_ms, delay_ms);

  if ((lightbulb_state.lightness_current == request->ctl.lightness)
      && (lightbulb_state.temperature_current == request->ctl.temperature)
      && (lightbulb_state.deltauv_current == request->ctl.deltauv)) {
    log("Request for current state received; no op\r\n");
  } else {
    if (lightbulb_state.lightness_current != request->ctl.lightness) {
      log("Setting lightness to <%u>\r\n", request->lightness);
    }
    if (lightbulb_state.temperature_current != request->ctl.temperature) {
      log("Setting temperature to <%u>\r\n", request->ctl.temperature);
    }
    if (lightbulb_state.deltauv_current != request->ctl.deltauv) {
      log("Setting delta UV to <%d>\r\n", request->ctl.deltauv);
    }
    if (transition_ms == 0 && delay_ms == 0) { // Immediate change
      lightbulb_state.lightness_current = request->ctl.lightness;
      lightbulb_state.lightness_target = request->ctl.lightness;
      if (request->lightness != 0) {
        lightbulb_state.lightness_last = request->ctl.lightness;
      }

      // update LED PWM duty cycle
      LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);

      lightbulb_state.temperature_current = request->ctl.temperature;
      lightbulb_state.temperature_target = request->ctl.temperature;
      lightbulb_state.deltauv_current = request->ctl.deltauv;
      lightbulb_state.deltauv_target = request->ctl.deltauv;

      // update LED color temperature
      LEDS_SetTemperature(lightbulb_state.temperature_current, lightbulb_state.deltauv_current, IMMEDIATE);
    } else if (delay_ms > 0) {
      // a delay has been specified for the light change. Start a soft timer
      // that will trigger the change after the given delay
      // Current state remains as is for now
      lightbulb_state.lightness_target = request->ctl.lightness;
      lightbulb_state.temperature_target = request->ctl.temperature;
      lightbulb_state.deltauv_target = request->ctl.deltauv;
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms), TIMER_ID_DELAYED_CTL, 1);
      // store transition parameter for later use
      delayed_ctl_trans = transition_ms;
    } else {
      // no delay but transition time has been set.
      lightbulb_state.lightness_target = request->ctl.lightness;
      lightbulb_state.temperature_target = request->ctl.temperature;
      lightbulb_state.deltauv_target = request->ctl.deltauv;

      LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);
      LEDS_SetTemperature(lightbulb_state.temperature_target, lightbulb_state.deltauv_target, transition_ms);

      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms), TIMER_ID_CTL_TRANSITION, 1);
    }
    lightbulb_state_changed();
    // State has changed, so the current scene number is reset
    gecko_cmd_mesh_scene_server_reset_register(element_index);
  }

  uint32_t remaining_ms = delay_ms + transition_ms;
  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    ctl_response(element_index, client_addr, appkey_index, remaining_ms);
  }
  ctl_update_and_publish(element_index, remaining_ms);
  // publish to bound states
  mesh_lib_generic_server_publish(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_lighting_state_lightness_actual);
  mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_TEMPERATURE_SERVER_MODEL_ID,
                                  element_index + 1,
                                  mesh_lighting_state_ctl_temperature);
}

/***************************************************************************//**
 * This function is a handler for light CTL change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void ctl_change(uint16_t model_id,
                       uint16_t element_index,
                       const struct mesh_generic_state *current,
                       const struct mesh_generic_state *target,
                       uint32_t remaining_ms)
{
  if (current->kind != mesh_lighting_state_ctl) {
    // if kind is not 'ctl' then just report the change here
    log("ctl change, kind %u\r\n", current->kind);
    return;
  }

  if (lightbulb_state.lightness_current != current->ctl.lightness) {
    log("lightness_change: from %u to %u\r\n", lightbulb_state.lightness_current, current->ctl.lightness);
    lightbulb_state.lightness_current = current->ctl.lightness;
    lightbulb_state_changed();
  } else {
    log("lightness update -same value (%u)\r\n", lightbulb_state.lightness_current);
  }

  if (lightbulb_state.temperature_current != current->ctl.temperature) {
    log("temperature_change: from %u to %u\r\n", lightbulb_state.temperature_current, current->ctl.temperature);
    lightbulb_state.temperature_current = current->ctl.temperature;
    lightbulb_state_changed();
  } else {
    log("temperature update -same value (%u)\r\n", lightbulb_state.temperature_current);
  }

  if (lightbulb_state.deltauv_current != current->ctl.deltauv) {
    log("deltauv_change: from %d to %d\r\n", lightbulb_state.deltauv_current, current->ctl.deltauv);
    lightbulb_state.deltauv_current = current->ctl.deltauv;
    lightbulb_state_changed();
  } else {
    log("deltauv update -same value (%d)\r\n", lightbulb_state.deltauv_current);
  }
}

/***************************************************************************//**
 * This function is a handler for light CTL recall event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
static void ctl_recall(uint16_t model_id,
                       uint16_t element_index,
                       const struct mesh_generic_state *current,
                       const struct mesh_generic_state *target,
                       uint32_t transition_ms)
{
  log("Light CTL recall\r\n");
  if (transition_ms == IMMEDIATE) {
    lightbulb_state.lightness_target = current->ctl.lightness;
    lightbulb_state.temperature_target = current->ctl.temperature;
    lightbulb_state.deltauv_target = current->ctl.deltauv;
  } else {
    lightbulb_state.lightness_target = target->ctl.lightness;
    lightbulb_state.temperature_target = target->ctl.temperature;
    lightbulb_state.deltauv_target = target->ctl.deltauv;
  }

  // Lightness server is mandatory for CTL, so lightness change is handled
  // in lightness_recall function and here we handle temperature and deltauv
  if ((lightbulb_state.temperature_current == lightbulb_state.temperature_target)
      && (lightbulb_state.deltauv_current == lightbulb_state.deltauv_target)) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall ctl temperature to %u, deltauv to %d with transition=%lu ms\r\n",
        lightbulb_state.temperature_target,
        lightbulb_state.deltauv_target,
        transition_ms);
    LEDS_SetTemperature(lightbulb_state.temperature_target,
                        lightbulb_state.deltauv_target,
                        transition_ms);

    if (transition_ms == IMMEDIATE) {
      lightbulb_state.lightness_current = current->ctl.lightness;
      lightbulb_state.temperature_current = current->ctl.temperature;
      lightbulb_state.deltauv_current = current->ctl.deltauv;
    } else {
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_CTL_TRANSITION,
                                        1);
    }
    lightbulb_state_changed();
  }

  // Lightness substate is updated in lightness_recall, here only temperature
  // substate is updated, it is needed also for LC recall to not set LC mode
  // to zero by bindings
  errorcode_t e;
  e = ctl_temperature_update(element_index + 1, transition_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_lighting_state_ctl);
  }
}

/***************************************************************************//**
 * This function is called when a light CTL request
 * with non-zero transition time has completed.
 ******************************************************************************/
static void ctl_transition_complete(void)
{
  // transition done -> set state, update and publish
  lightbulb_state.lightness_current = lightbulb_state.lightness_target;
  lightbulb_state.temperature_current = lightbulb_state.temperature_target;
  lightbulb_state.deltauv_current = lightbulb_state.deltauv_target;

  log("transition complete. New lightness is %u, new temperature is %u and new deltauv is %d\r\n", lightbulb_state.lightness_current, lightbulb_state.temperature_current, lightbulb_state.deltauv_current);

  lightbulb_state_changed();
  ctl_update_and_publish(_primary_elem_index, IMMEDIATE);
}

/***************************************************************************//**
 * This function is called when delay for light CTL request has completed.
 ******************************************************************************/
static void delayed_ctl_request(void)
{
  log("starting delayed ctl request: lightness %u -> %u, temperature %u -> %u, deltauv %d -> %d, %lu ms\r\n",
      lightbulb_state.lightness_current,
      lightbulb_state.lightness_target,
      lightbulb_state.temperature_current,
      lightbulb_state.temperature_target,
      lightbulb_state.deltauv_current,
      lightbulb_state.deltauv_target,
      delayed_ctl_trans
      );

  LEDS_SetLevel(lightbulb_state.lightness_target, delayed_ctl_trans);
  LEDS_SetTemperature(lightbulb_state.temperature_target, lightbulb_state.deltauv_target, delayed_ctl_trans);

  if (delayed_ctl_trans == 0) {
    // no transition delay, update state immediately
    lightbulb_state.lightness_current = lightbulb_state.lightness_target;
    lightbulb_state.temperature_current = lightbulb_state.temperature_target;
    lightbulb_state.deltauv_current = lightbulb_state.deltauv_target;

    lightbulb_state_changed();
    ctl_update_and_publish(_primary_elem_index, delayed_ctl_trans);
  } else {
    // state is updated when transition is complete
    gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_ctl_trans), TIMER_ID_CTL_TRANSITION, 1);
  }
}

/** @} (end addtogroup LightCTL) */

/***********************************************************************************************//**
 * \defgroup LightCTLSetup
 * \brief Light CTL Setup Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup LightCTLSetup
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to light CTL setup request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] kind           Type of state used in light CTL setup response.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_setup_response(uint16_t element_index,
                                      uint16_t client_addr,
                                      uint16_t appkey_index,
                                      mesh_generic_state_t kind)
{
  struct mesh_generic_state current;

  current.kind = kind;

  switch (kind) {
    case mesh_lighting_state_ctl_default:
      current.ctl.lightness = lightbulb_state.lightness_default;
      current.ctl.temperature = lightbulb_state.temperature_default;
      current.ctl.deltauv = lightbulb_state.deltauv_default;
      break;
    case mesh_lighting_state_ctl_temperature_range:
      current.ctl_temperature_range.min = lightbulb_state.temperature_min;
      current.ctl_temperature_range.max = lightbulb_state.temperature_max;
      break;
    default:
      break;
  }

  return mesh_lib_generic_server_response(MESH_LIGHTING_CTL_SETUP_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          NULL,
                                          0,
                                          0x00);
}

/***************************************************************************//**
 * Update light CTL setup state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] kind           Type of state used in light CTL setup update.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_setup_update(uint16_t element_index, mesh_generic_state_t kind)
{
  struct mesh_generic_state current;

  current.kind = kind;

  switch (kind) {
    case mesh_lighting_state_ctl_default:
      current.ctl.lightness = lightbulb_state.lightness_default;
      current.ctl.temperature = lightbulb_state.temperature_default;
      current.ctl.deltauv = lightbulb_state.deltauv_default;
      break;
    case mesh_lighting_state_ctl_temperature_range:
      current.ctl_temperature_range.min = lightbulb_state.temperature_min;
      current.ctl_temperature_range.max = lightbulb_state.temperature_max;
      break;
    default:
      break;
  }

  return mesh_lib_generic_server_update(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        NULL,
                                        0);
}

/***************************************************************************//**
 * This function process the requests for the light CTL setup model.
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
static void ctl_setup_request(uint16_t model_id,
                              uint16_t element_index,
                              uint16_t client_addr,
                              uint16_t server_addr,
                              uint16_t appkey_index,
                              const struct mesh_generic_request *request,
                              uint32_t transition_ms,
                              uint16_t delay_ms,
                              uint8_t request_flags)
{
  mesh_generic_state_t kind = mesh_generic_state_last;
  switch (request->kind) {
    case mesh_lighting_request_ctl_default:
      kind = mesh_lighting_state_ctl_default;
      log("ctl_setup_request: state=ctl_default, default_lightness=%u, default_temperature=%u, default_delta_uv=%d\r\n",
          request->ctl.lightness, request->ctl.temperature, request->ctl.deltauv);

      if ((lightbulb_state.lightness_default == request->ctl.lightness)
          && (lightbulb_state.temperature_default == request->ctl.temperature)
          && (lightbulb_state.deltauv_default == request->ctl.deltauv)) {
        log("Request for current state received; no op\r\n");
      } else {
        if (lightbulb_state.lightness_default != request->ctl.lightness) {
          log("Setting default lightness to <%u>\r\n", request->ctl.lightness);
          lightbulb_state.lightness_default = request->ctl.lightness;
        }
        if (lightbulb_state.temperature_default != request->ctl.temperature) {
          log("Setting default temperature to <%u>\r\n", request->ctl.temperature);
          lightbulb_state.temperature_default = request->ctl.temperature;
        }
        if (lightbulb_state.deltauv_default != request->ctl.deltauv) {
          log("Setting default delta UV to <%d>\r\n", request->ctl.deltauv);
          lightbulb_state.deltauv_default = request->ctl.deltauv;
        }
        lightbulb_state_changed();
      }
      break;

    case mesh_lighting_request_ctl_temperature_range:
      kind = mesh_lighting_state_ctl_temperature_range;
      log("ctl_setup_request: state=ctl_temperature_range, min_temperature=%u, max_temperature=%u\r\n",
          request->ctl_temperature_range.min, request->ctl_temperature_range.max);

      if ((lightbulb_state.temperature_min == request->ctl_temperature_range.min)
          && (lightbulb_state.temperature_max == request->ctl_temperature_range.max)) {
        log("Request for current state received; no op\r\n");
      } else {
        if (lightbulb_state.temperature_min != request->ctl_temperature_range.min) {
          log("Setting min temperature to <%u>\r\n", request->ctl_temperature_range.min);
          lightbulb_state.temperature_min = request->ctl_temperature_range.min;
        }
        if (lightbulb_state.temperature_max != request->ctl_temperature_range.max) {
          log("Setting max temperature to <%u>\r\n", request->ctl_temperature_range.max);
          lightbulb_state.temperature_max = request->ctl_temperature_range.max;
        }
        lightbulb_state_changed();
      }
      break;

    default:
      break;
  }

  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    ctl_setup_response(element_index, client_addr, appkey_index, kind);
  } else {
    ctl_setup_update(element_index, kind);
  }
}

/***************************************************************************//**
 * This function is a handler for light CTL setup change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void ctl_setup_change(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t remaining_ms)
{
  switch (current->kind) {
    case mesh_lighting_state_ctl_default:
      if (lightbulb_state.lightness_default != current->ctl.lightness) {
        log("default_lightness_change: from %u to %u\r\n", lightbulb_state.lightness_default, current->ctl.lightness);
        lightbulb_state.lightness_default = current->ctl.lightness;
        lightbulb_state_changed();
      } else {
        log("default lightness update -same value (%u)\r\n", lightbulb_state.lightness_default);
      }

      if (lightbulb_state.temperature_default != current->ctl.temperature) {
        log("default_temperature_change: from %u to %u\r\n", lightbulb_state.temperature_default, current->ctl.temperature);
        lightbulb_state.temperature_default = current->ctl.temperature;
        lightbulb_state_changed();
      } else {
        log("default temperature update -same value (%u)\r\n", lightbulb_state.temperature_default);
      }

      if (lightbulb_state.deltauv_default != current->ctl.deltauv) {
        log("default_deltauv_change: from %d to %d\r\n", lightbulb_state.deltauv_default, current->ctl.deltauv);
        lightbulb_state.deltauv_default = current->ctl.deltauv;
        lightbulb_state_changed();
      } else {
        log("default deltauv update -same value (%d)\r\n", lightbulb_state.deltauv_default);
      }

      break;

    case mesh_lighting_state_ctl_temperature_range:
      if (lightbulb_state.temperature_min != current->ctl_temperature_range.min) {
        log("min_temperature_change: from %u to %u\r\n", lightbulb_state.temperature_min, current->ctl_temperature_range.min);
        lightbulb_state.temperature_min = current->ctl_temperature_range.min;
        lightbulb_state_changed();
      } else {
        log("min temperature update -same value (%u)\r\n", lightbulb_state.temperature_min);
      }

      if (lightbulb_state.temperature_max != current->ctl_temperature_range.max) {
        log("max_temperature_change: from %u to %u\r\n", lightbulb_state.temperature_max, current->ctl_temperature_range.max);
        lightbulb_state.temperature_max = current->ctl_temperature_range.max;
        lightbulb_state_changed();
      } else {
        log("max temperature update -same value (%u)\r\n", lightbulb_state.temperature_max);
      }

      break;

    default:
      break;
  }
}

/** @} (end addtogroup LightCTLSetup) */

/***********************************************************************************************//**
 * \defgroup LightCTLTemperature
 * \brief Light CTL Temperature Server model.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup LightCTLTemperature
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Response to light CTL temperature request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_temperature_response(uint16_t element_index,
                                            uint16_t client_addr,
                                            uint16_t appkey_index,
                                            uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_lighting_state_ctl_temperature;
  current.ctl_temperature.temperature = lightbulb_state.temperature_current;
  current.ctl_temperature.deltauv = lightbulb_state.deltauv_current;

  target.kind = mesh_lighting_state_ctl_temperature;
  target.ctl_temperature.temperature = lightbulb_state.temperature_target;
  target.ctl_temperature.deltauv = lightbulb_state.deltauv_target;

  return mesh_lib_generic_server_response(MESH_LIGHTING_CTL_TEMPERATURE_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          &target,
                                          remaining_ms,
                                          0x00);
}

/***************************************************************************//**
 * Update light CTL temperature state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_temperature_update(uint16_t element_index,
                                          uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_lighting_state_ctl_temperature;
  current.ctl_temperature.temperature = lightbulb_state.temperature_current;
  current.ctl_temperature.deltauv = lightbulb_state.deltauv_current;

  target.kind = mesh_lighting_state_ctl_temperature;
  target.ctl_temperature.temperature = lightbulb_state.temperature_target;
  target.ctl_temperature.deltauv = lightbulb_state.deltauv_target;

  return mesh_lib_generic_server_update(MESH_LIGHTING_CTL_TEMPERATURE_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update light CTL temperature state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t ctl_temperature_update_and_publish(uint16_t element_index,
                                                      uint32_t remaining_ms)
{
  errorcode_t e;

  e = ctl_temperature_update(element_index, remaining_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_TEMPERATURE_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_lighting_state_ctl_temperature);
  }

  return e;
}

/***************************************************************************//**
 * This function process the requests for the light CTL temperature model.
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
static void ctl_temperature_request(uint16_t model_id,
                                    uint16_t element_index,
                                    uint16_t client_addr,
                                    uint16_t server_addr,
                                    uint16_t appkey_index,
                                    const struct mesh_generic_request *request,
                                    uint32_t transition_ms,
                                    uint16_t delay_ms,
                                    uint8_t request_flags)
{
  log("ctl_temperature_request: temperature=%u, delta_uv=%d, transition=%lu, delay=%u\r\n",
      request->ctl_temperature.temperature, request->ctl_temperature.deltauv, transition_ms, delay_ms);

  if ((lightbulb_state.temperature_current == request->ctl_temperature.temperature)
      && (lightbulb_state.deltauv_current == request->ctl_temperature.deltauv)) {
    log("Request for current state received; no op\r\n");
  } else {
    if (lightbulb_state.temperature_current != request->ctl_temperature.temperature) {
      log("Setting temperature to <%u>\r\n", request->ctl_temperature.temperature);
    }
    if (lightbulb_state.deltauv_current != request->ctl_temperature.deltauv) {
      log("Setting delta UV to <%d>\r\n", request->ctl_temperature.deltauv);
    }
    if (transition_ms == 0 && delay_ms == 0) { // Immediate change
      lightbulb_state.temperature_current = request->ctl_temperature.temperature;
      lightbulb_state.temperature_target = request->ctl_temperature.temperature;
      lightbulb_state.deltauv_current = request->ctl_temperature.deltauv;
      lightbulb_state.deltauv_target = request->ctl_temperature.deltauv;

      // update LED color temperature
      LEDS_SetTemperature(lightbulb_state.temperature_current, lightbulb_state.deltauv_current, IMMEDIATE);
    } else if (delay_ms > 0) {
      // a delay has been specified for the temperature change. Start a soft timer
      // that will trigger the change after the given delay
      // Current state remains as is for now
      lightbulb_state.temperature_target = request->ctl_temperature.temperature;
      lightbulb_state.deltauv_target = request->ctl_temperature.deltauv;
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms), TIMER_ID_DELAYED_CTL_TEMPERATURE, 1);
      // store transition parameter for later use
      delayed_ctl_temperature_trans = transition_ms;
    } else {
      // no delay but transition time has been set.
      lightbulb_state.temperature_target = request->ctl_temperature.temperature;
      lightbulb_state.deltauv_target = request->ctl_temperature.deltauv;

      LEDS_SetTemperature(lightbulb_state.temperature_target, lightbulb_state.deltauv_target, transition_ms);

      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms), TIMER_ID_CTL_TEMP_TRANSITION, 1);
    }
    lightbulb_state_changed();
    // State has changed, so the current scene number is reset
    gecko_cmd_mesh_scene_server_reset_register(element_index);
  }

  uint32_t remaining_ms = delay_ms + transition_ms;
  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    ctl_temperature_response(element_index, client_addr, appkey_index, remaining_ms);
  }
  ctl_temperature_update_and_publish(element_index, remaining_ms);
  // publish to bound states
  mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                  element_index - 1,
                                  mesh_lighting_state_ctl);
  mesh_lib_generic_server_publish(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_generic_state_level);
}

/***************************************************************************//**
 * This function is a handler for light CTL temperature change event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void ctl_temperature_change(uint16_t model_id,
                                   uint16_t element_index,
                                   const struct mesh_generic_state *current,
                                   const struct mesh_generic_state *target,
                                   uint32_t remaining_ms)
{
  if (lightbulb_state.temperature_current != current->ctl_temperature.temperature) {
    log("temperature_change: from %u to %u\r\n", lightbulb_state.temperature_current, current->ctl_temperature.temperature);
    lightbulb_state.temperature_current = current->ctl_temperature.temperature;
    lightbulb_state_changed();
  } else {
    log("temperature update -same value (%u)\r\n", lightbulb_state.temperature_current);
  }

  if (lightbulb_state.deltauv_current != current->ctl_temperature.deltauv) {
    log("deltauv_change: from %d to %d\r\n", lightbulb_state.deltauv_current, current->ctl_temperature.deltauv);
    lightbulb_state.deltauv_current = current->ctl_temperature.deltauv;
    lightbulb_state_changed();
  } else {
    log("deltauv update -same value (%d)\r\n", lightbulb_state.deltauv_current);
  }
}

/***************************************************************************//**
 * This function is a handler for light CTL temperature recall event.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
static void ctl_temperature_recall(uint16_t model_id,
                                   uint16_t element_index,
                                   const struct mesh_generic_state *current,
                                   const struct mesh_generic_state *target,
                                   uint32_t transition_ms)
{
  log("CTL Temperature recall\r\n");
  if (transition_ms == IMMEDIATE) {
    lightbulb_state.temperature_target = current->ctl_temperature.temperature;
    lightbulb_state.deltauv_target = current->ctl_temperature.deltauv;
  } else {
    lightbulb_state.temperature_target = target->ctl_temperature.temperature;
    lightbulb_state.deltauv_target = target->ctl_temperature.deltauv;
  }

  if ((lightbulb_state.temperature_current == lightbulb_state.temperature_target)
      && (lightbulb_state.deltauv_current == lightbulb_state.deltauv_target)) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall ctl temperature to %u, deltauv to %d with transition=%lu ms\r\n",
        lightbulb_state.temperature_target,
        lightbulb_state.deltauv_target,
        transition_ms);
    LEDS_SetTemperature(lightbulb_state.temperature_target,
                        lightbulb_state.deltauv_target,
                        transition_ms);

    if (transition_ms == IMMEDIATE) {
      lightbulb_state.temperature_current = current->ctl_temperature.temperature;
      lightbulb_state.deltauv_current = current->ctl_temperature.deltauv;
    } else {
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_CTL_TEMP_TRANSITION,
                                        1);
    }
    lightbulb_state_changed();
  }

  ctl_temperature_update_and_publish(element_index, transition_ms);
}

/***************************************************************************//**
 * This function is called when a light CTL temperature request
 * with non-zero transition time has completed.
 ******************************************************************************/
static void ctl_temperature_transition_complete(void)
{
  // transition done -> set state, update and publish
  lightbulb_state.temperature_current = lightbulb_state.temperature_target;
  lightbulb_state.deltauv_current = lightbulb_state.deltauv_target;

  log("transition complete. New temperature is %u and new deltauv is %d\r\n", lightbulb_state.temperature_current, lightbulb_state.deltauv_current);

  lightbulb_state_changed();
  ctl_temperature_update_and_publish(_secondary_elem_index, IMMEDIATE);
}

/***************************************************************************//**
 * This function is called when delay for light CTL temperature request
 * has completed.
 ******************************************************************************/
static void delayed_ctl_temperature_request(void)
{
  log("starting delayed ctl temperature request: temperature %u -> %u, deltauv %d -> %d, %lu ms\r\n",
      lightbulb_state.temperature_current,
      lightbulb_state.temperature_target,
      lightbulb_state.deltauv_current,
      lightbulb_state.deltauv_target,
      delayed_ctl_temperature_trans
      );

  LEDS_SetTemperature(lightbulb_state.temperature_target, lightbulb_state.deltauv_target, delayed_ctl_temperature_trans);

  if (delayed_ctl_temperature_trans == 0) {
    // no transition delay, update state immediately
    lightbulb_state.temperature_current = lightbulb_state.temperature_target;
    lightbulb_state.deltauv_current = lightbulb_state.deltauv_target;

    lightbulb_state_changed();
    ctl_temperature_update_and_publish(_secondary_elem_index, delayed_ctl_temperature_trans);
  } else {
    // state is updated when transition is complete
    gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_ctl_temperature_trans), TIMER_ID_CTL_TEMP_TRANSITION, 1);
  }
}

/** @} (end addtogroup LightCTLTemperature) */

/***********************************************************************************************//**
 * \defgroup SecGenericLevel
 * \brief Generic Level Server model on secondary element.
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup SecGenericLevel
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * Convert level to temperature.
 *
 * @param[in] level  Level to convert.
 *
 * @return Temperature converted from level.
 ******************************************************************************/
static uint16_t level_to_temperature(int16_t level)
{
  return lightbulb_state.temperature_min                                       \
         + (uint32_t)(level + (int32_t)32768)                                  \
         * (lightbulb_state.temperature_max - lightbulb_state.temperature_min) \
         / 65535;
}

/***************************************************************************//**
 * Convert temperature to level.
 *
 * @param[in] temperature  Temperature to convert.
 *
 * @return Level converted from temperature.
 ******************************************************************************/
static int16_t temperature_to_level(uint16_t temperature)
{
  return (temperature - lightbulb_state.temperature_min)                       \
         * (uint32_t)65535                                                     \
         / (lightbulb_state.temperature_max - lightbulb_state.temperature_min) \
         - (int32_t)32768;
}

/***************************************************************************//**
 * Response to generic level request on secondary element.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t sec_level_response(uint16_t element_index,
                                      uint16_t client_addr,
                                      uint16_t appkey_index,
                                      uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_level;
  current.level.level = lightbulb_state.sec_level_current;

  target.kind = mesh_generic_state_level;
  target.level.level = lightbulb_state.sec_level_target;

  return mesh_lib_generic_server_response(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                          element_index,
                                          client_addr,
                                          appkey_index,
                                          &current,
                                          &target,
                                          remaining_ms,
                                          0x00);
}

/***************************************************************************//**
 * Update generic level state on secondary element.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t sec_level_update(uint16_t element_index,
                                    uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_level;
  current.level.level = lightbulb_state.sec_level_current;

  target.kind = mesh_generic_state_level;
  target.level.level = lightbulb_state.sec_level_target;

  return mesh_lib_generic_server_update(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update generic level state on secondary element
 * and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t sec_level_update_and_publish(uint16_t element_index,
                                                uint32_t remaining_ms)
{
  errorcode_t e;

  e = sec_level_update(element_index, remaining_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_generic_state_level);
  }

  return e;
}

/***************************************************************************//**
 * Schedule next generic level move request on secondary element.
 *
 * @param[in] remaining_delta   The remaining level delta to the target state.
 ******************************************************************************/
static void sec_level_move_schedule_next_request(int32_t remaining_delta)
{
  uint32_t transition_ms = 0;
  if (abs(remaining_delta) < abs(move_sec_level_delta)) {
    transition_ms = (uint32_t)(((int64_t)move_sec_level_trans * remaining_delta)
                               / move_sec_level_delta);
    LEDS_SetTemperature(lightbulb_state.temperature_target,
                        lightbulb_state.deltauv_current,
                        transition_ms);
  } else {
    int16_t next_level = lightbulb_state.sec_level_current + move_sec_level_delta;
    transition_ms = move_sec_level_trans;
    LEDS_SetTemperature(level_to_temperature(next_level),
                        lightbulb_state.deltauv_current,
                        move_sec_level_trans);
  }
  gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                    TIMER_ID_SEC_LEVEL_MOVE,
                                    1);
}

/***************************************************************************//**
 * Handle generic level move request on secondary element.
 ******************************************************************************/
static void sec_level_move_request(void)
{
  log("secondary level move: level %d -> %d, delta %d in %lu ms\r\n",
      lightbulb_state.sec_level_current,
      lightbulb_state.sec_level_target,
      move_sec_level_delta,
      move_sec_level_trans);

  int32_t remaining_delta = (int32_t)lightbulb_state.sec_level_target
                            - lightbulb_state.sec_level_current;

  if (abs(remaining_delta) < abs(move_sec_level_delta)) {
    // end of move level as it reached target state
    lightbulb_state.sec_level_current = lightbulb_state.sec_level_target;
    lightbulb_state.temperature_current = lightbulb_state.temperature_target;
  } else {
    lightbulb_state.sec_level_current += move_sec_level_delta;
    uint16_t temperature = level_to_temperature(lightbulb_state.sec_level_current);
    lightbulb_state.temperature_current = temperature;
  }
  lightbulb_state_changed();
  sec_level_update_and_publish(_secondary_elem_index, UNKNOWN_REMAINING_TIME);

  remaining_delta = (int32_t)lightbulb_state.sec_level_target
                    - lightbulb_state.sec_level_current;
  if (remaining_delta != 0) {
    sec_level_move_schedule_next_request(remaining_delta);
  }
}

/***************************************************************************//**
 * Stop generic level move on secondary element.
 ******************************************************************************/
static void sec_level_move_stop(void)
{
  // Cancel timers
  gecko_cmd_hardware_set_soft_timer(0, TIMER_ID_DELAYED_SEC_LEVEL, 1);
  gecko_cmd_hardware_set_soft_timer(0, TIMER_ID_SEC_LEVEL_MOVE, 1);
  //Reset move parameters
  move_sec_level_delta = 0;
  move_sec_level_trans = 0;
}

/***************************************************************************//**
 * This function process the requests for the generic level model
 * on secondary element.
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
static void sec_level_request(uint16_t model_id,
                              uint16_t element_index,
                              uint16_t client_addr,
                              uint16_t server_addr,
                              uint16_t appkey_index,
                              const struct mesh_generic_request *request,
                              uint32_t transition_ms,
                              uint16_t delay_ms,
                              uint8_t request_flags)
{
  uint16_t temperature;
  uint32_t remaining_ms = UNKNOWN_REMAINING_TIME;

  switch (request->kind) {
    case mesh_generic_request_level:
      log("sec_level_request: level=%d, transition=%lu, delay=%u\r\n",
          request->level, transition_ms, delay_ms);

      sec_level_move_stop();
      if (lightbulb_state.sec_level_current == request->level) {
        log("Request for current state received; no op\r\n");
        lightbulb_state.sec_level_target = request->level;
      } else {
        log("Setting sec_level to <%d>\r\n", request->level);

        temperature = level_to_temperature(request->level);

        if (transition_ms == 0 && delay_ms == 0) { // Immediate change
          lightbulb_state.sec_level_current = request->level;
          lightbulb_state.sec_level_target = request->level;
          lightbulb_state.temperature_current = temperature;
          lightbulb_state.temperature_target = temperature;

          // update LED Temperature
          LEDS_SetTemperature(temperature,
                              lightbulb_state.deltauv_current,
                              IMMEDIATE);
        } else if (delay_ms > 0) {
          // a delay has been specified for the change. Start a soft timer
          // that will trigger the change after the given delay
          // Current state remains as is for now
          lightbulb_state.sec_level_target = request->level;
          lightbulb_state.temperature_target = temperature;
          sec_level_request_kind = mesh_generic_request_level;
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                            TIMER_ID_DELAYED_SEC_LEVEL,
                                            1);
          // store transition parameter for later use
          delayed_sec_level_trans = transition_ms;
        } else {
          // no delay but transition time has been set.
          lightbulb_state.sec_level_target = request->level;
          lightbulb_state.temperature_target = temperature;
          LEDS_SetTemperature(temperature,
                              lightbulb_state.deltauv_current,
                              transition_ms);

          // lightbulb current state will be updated when transition is complete
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                            TIMER_ID_SEC_LEVEL_TRANSITION,
                                            1);
        }
        // State has changed, so the current scene number is reset
        gecko_cmd_mesh_scene_server_reset_register(element_index);
      }

      remaining_ms = delay_ms + transition_ms;
      break;

    case mesh_generic_request_level_move:
      log("sec_level_move_request: delta=%d, transition=%lu, delay=%u\r\n",
          request->level, transition_ms, delay_ms);
      // Store move parameters
      move_sec_level_delta = request->level;
      move_sec_level_trans = transition_ms;

      int16_t requested_level = 0;
      if (move_sec_level_delta > 0) {
        requested_level = 0x7FFF; // Max level value
      } else if (move_sec_level_delta < 0) {
        requested_level = 0x8000; // Min level value
      }

      if (lightbulb_state.sec_level_current == requested_level) {
        log("Request for current state received; no op\r\n");
        lightbulb_state.sec_level_target = requested_level;
        remaining_ms = IMMEDIATE;
      } else {
        log("Setting sec_level to <%d>\r\n", requested_level);

        temperature = level_to_temperature(requested_level);

        if (delay_ms > 0) {
          // a delay has been specified for the move. Start a soft timer
          // that will trigger the move after the given delay
          // Current state remains as is for now
          lightbulb_state.sec_level_target = requested_level;
          lightbulb_state.temperature_target = temperature;
          sec_level_request_kind = mesh_generic_request_level_move;
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                            TIMER_ID_DELAYED_SEC_LEVEL,
                                            1);
        } else {
          // no delay so start move
          lightbulb_state.sec_level_target = requested_level;
          lightbulb_state.temperature_target = temperature;

          int32_t remaining_delta = (int32_t)lightbulb_state.sec_level_target
                                    - lightbulb_state.sec_level_current;
          sec_level_move_schedule_next_request(remaining_delta);
        }
        remaining_ms = UNKNOWN_REMAINING_TIME;
        // State has changed, so the current scene number is reset
        gecko_cmd_mesh_scene_server_reset_register(element_index);
      }
      break;

    case mesh_generic_request_level_halt:
      log("sec_level_halt_request\r\n");

      // Set current state
      lightbulb_state.temperature_current = LEDS_GetTemperature();
      lightbulb_state.temperature_target = lightbulb_state.temperature_current;
      lightbulb_state.sec_level_current = temperature_to_level(lightbulb_state.temperature_current);
      lightbulb_state.sec_level_target = lightbulb_state.sec_level_current;
      if (delay_ms > 0) {
        // a delay has been specified for the move halt. Start a soft timer
        // that will trigger the move halt after the given delay
        // Current state remains as is for now
        remaining_ms = delay_ms;
        sec_level_request_kind = mesh_generic_request_level_halt;
        gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                          TIMER_ID_DELAYED_SEC_LEVEL,
                                          1);
      } else {
        sec_level_move_stop();
        LEDS_SetTemperature(lightbulb_state.temperature_current,
                            lightbulb_state.deltauv_current,
                            IMMEDIATE);
        remaining_ms = IMMEDIATE;
      }
      break;

    default:
      break;
  }

  lightbulb_state_changed();

  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    sec_level_response(element_index, client_addr, appkey_index, remaining_ms);
  }
  sec_level_update_and_publish(element_index, remaining_ms);
  // publish to bound states
  mesh_lib_generic_server_publish(MESH_LIGHTING_CTL_TEMPERATURE_SERVER_MODEL_ID,
                                  element_index,
                                  mesh_lighting_state_ctl_temperature);
}

/***************************************************************************//**
 * This function is a handler for generic level change event
 * on secondary element.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] remaining_ms   Time (in milliseconds) remaining before transition
 *                           from current state to target state is complete.
 ******************************************************************************/
static void sec_level_change(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t remaining_ms)
{
  if (lightbulb_state.sec_level_current != current->level.level) {
    log("sec_level_change: from %d to %d\r\n",
        lightbulb_state.sec_level_current,
        current->level.level);
    lightbulb_state.sec_level_current = current->level.level;
    lightbulb_state_changed();
    sec_level_move_stop();
  } else {
    log("sec_level update -same value (%d)\r\n",
        lightbulb_state.sec_level_current);
  }
}

/***************************************************************************//**
 * This function is a handler for generic level recall event
 * on secondary element.
 *
 * @param[in] model_id       Server model ID.
 * @param[in] element_index  Server model element index.
 * @param[in] current        Pointer to current state structure.
 * @param[in] target         Pointer to target state structure.
 * @param[in] transition_ms  Transition time (in milliseconds).
 ******************************************************************************/
static void sec_level_recall(uint16_t model_id,
                             uint16_t element_index,
                             const struct mesh_generic_state *current,
                             const struct mesh_generic_state *target,
                             uint32_t transition_ms)
{
  log("Secondary Generic Level recall\r\n");
  if (transition_ms == IMMEDIATE) {
    lightbulb_state.sec_level_target = current->level.level;
  } else {
    lightbulb_state.sec_level_target = target->level.level;
  }

  if (lightbulb_state.sec_level_current == lightbulb_state.sec_level_target) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall sec_level to %d with transition=%lu ms\r\n",
        lightbulb_state.sec_level_target,
        transition_ms);
    if (transition_ms == IMMEDIATE) {
      lightbulb_state.sec_level_current = current->level.level;
    } else {
      // lightbulb current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_SEC_LEVEL_TRANSITION,
                                        1);
    }
    lightbulb_state_changed();
  }
}

/***************************************************************************//**
 * This function is called when a generic level request on secondary element
 * with non-zero transition time has completed.
 ******************************************************************************/
static void sec_level_transition_complete(void)
{
  // transition done -> set state, update and publish
  lightbulb_state.sec_level_current = lightbulb_state.sec_level_target;
  lightbulb_state.temperature_current = lightbulb_state.temperature_target;

  log("transition complete. New sec_level is %d\r\n",
      lightbulb_state.sec_level_current);

  lightbulb_state_changed();
  sec_level_update_and_publish(_secondary_elem_index, IMMEDIATE);
}

/***************************************************************************//**
 * This function is called when delay for generic level request
 * on secondary element has completed.
 ******************************************************************************/
static void delayed_sec_level_request(void)
{
  log("starting delayed secondary level request: level %d -> %d, %lu ms\r\n",
      lightbulb_state.sec_level_current,
      lightbulb_state.sec_level_target,
      delayed_sec_level_trans);

  switch (sec_level_request_kind) {
    case mesh_generic_request_level:
      LEDS_SetTemperature(lightbulb_state.temperature_target,
                          lightbulb_state.deltauv_current,
                          delayed_sec_level_trans);

      if (delayed_sec_level_trans == 0) {
        // no transition delay, update state immediately
        lightbulb_state.sec_level_current = lightbulb_state.sec_level_target;
        lightbulb_state.temperature_current = lightbulb_state.temperature_target;

        lightbulb_state_changed();
        sec_level_update_and_publish(_secondary_elem_index,
                                     delayed_sec_level_trans);
      } else {
        // state is updated when transition is complete
        gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_sec_level_trans),
                                          TIMER_ID_SEC_LEVEL_TRANSITION,
                                          1);
      }
      break;

    case mesh_generic_request_level_move:
      sec_level_move_schedule_next_request((int32_t)lightbulb_state.sec_level_target
                                           - lightbulb_state.sec_level_current);
      sec_level_update_and_publish(_secondary_elem_index, UNKNOWN_REMAINING_TIME);
      break;

    case mesh_generic_request_level_halt:
      // Set current state
      lightbulb_state.temperature_current = LEDS_GetTemperature();
      lightbulb_state.temperature_target = lightbulb_state.temperature_current;
      lightbulb_state.sec_level_current = temperature_to_level(lightbulb_state.temperature_current);
      lightbulb_state.sec_level_target = lightbulb_state.sec_level_current;
      sec_level_move_stop();
      LEDS_SetTemperature(lightbulb_state.temperature_current,
                          lightbulb_state.deltauv_current,
                          IMMEDIATE);
      sec_level_update_and_publish(_secondary_elem_index, IMMEDIATE);
      break;

    default:
      break;
  }
}

/** @} (end addtogroup SecGenericLevel) */

/***************************************************************************//**
 * Initialization of the models supported by this node.
 * This function registers callbacks for each of the supported models.
 ******************************************************************************/
static void init_models(void)
{
  mesh_lib_generic_server_register_handler(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                           0,
                                           onoff_request,
                                           onoff_change,
                                           onoff_recall);
  mesh_lib_generic_server_register_handler(MESH_GENERIC_POWER_ON_OFF_SETUP_SERVER_MODEL_ID,
                                           0,
                                           power_onoff_request,
                                           power_onoff_change,
                                           NULL);
  mesh_lib_generic_server_register_handler(MESH_GENERIC_TRANSITION_TIME_SERVER_MODEL_ID,
                                           0,
                                           transtime_request,
                                           transtime_change,
                                           NULL);

  mesh_lib_generic_server_register_handler(MESH_LIGHTING_LIGHTNESS_SERVER_MODEL_ID,
                                           0,
                                           lightness_request,
                                           lightness_change,
                                           lightness_recall);
  mesh_lib_generic_server_register_handler(MESH_LIGHTING_LIGHTNESS_SETUP_SERVER_MODEL_ID,
                                           0,
                                           lightness_setup_request,
                                           lightness_setup_change,
                                           NULL);
  mesh_lib_generic_server_register_handler(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                           0,
                                           pri_level_request,
                                           pri_level_change,
                                           pri_level_recall);
  mesh_lib_generic_server_register_handler(MESH_LIGHTING_CTL_SERVER_MODEL_ID,
                                           0,
                                           ctl_request,
                                           ctl_change,
                                           ctl_recall);
  mesh_lib_generic_server_register_handler(MESH_LIGHTING_CTL_SETUP_SERVER_MODEL_ID,
                                           0,
                                           ctl_setup_request,
                                           ctl_setup_change,
                                           NULL);
  mesh_lib_generic_server_register_handler(MESH_LIGHTING_CTL_TEMPERATURE_SERVER_MODEL_ID,
                                           1,
                                           ctl_temperature_request,
                                           ctl_temperature_change,
                                           ctl_temperature_recall);
  mesh_lib_generic_server_register_handler(MESH_GENERIC_LEVEL_SERVER_MODEL_ID,
                                           1,
                                           sec_level_request,
                                           sec_level_change,
                                           sec_level_recall);

  mesh_lib_generic_server_register_handler(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                           1,
                                           lc_onoff_request,
                                           lc_onoff_change,
                                           lc_onoff_recall);
}

/** @} (end addtogroup mesh_models) */

/***********************************************************************************************//**
 * @addtogroup lightbulb_state
 * @{
 **************************************************************************************************/

/***************************************************************************//**
 * This function validates the lighbulb_state and change it if it is against
 * the specification.
 ******************************************************************************/
static void lightbulb_state_validate_and_correct(void)
{
  if (lightbulb_state.lightness_min > lightbulb_state.lightness_max) {
    lightbulb_state.lightness_min = lightbulb_state.lightness_max;
  }
  if (lightbulb_state.lightness_default) {
    if (lightbulb_state.lightness_default < lightbulb_state.lightness_min) {
      lightbulb_state.lightness_default = lightbulb_state.lightness_min;
    }
    if (lightbulb_state.lightness_default > lightbulb_state.lightness_max) {
      lightbulb_state.lightness_default = lightbulb_state.lightness_max;
    }
  }
  if (lightbulb_state.lightness_current < lightbulb_state.lightness_min) {
    lightbulb_state.lightness_current = lightbulb_state.lightness_min;
  }
  if (lightbulb_state.lightness_current > lightbulb_state.lightness_max) {
    lightbulb_state.lightness_current = lightbulb_state.lightness_max;
  }
  if (lightbulb_state.lightness_target < lightbulb_state.lightness_min) {
    lightbulb_state.lightness_target = lightbulb_state.lightness_min;
  }
  if (lightbulb_state.lightness_target > lightbulb_state.lightness_max) {
    lightbulb_state.lightness_target = lightbulb_state.lightness_max;
  }

  if (lightbulb_state.temperature_min < MIN_TEMPERATURE) {
    lightbulb_state.temperature_min = MIN_TEMPERATURE;
  }
  if (lightbulb_state.temperature_min > MAX_TEMPERATURE) {
    lightbulb_state.temperature_min = MAX_TEMPERATURE;
  }
  if (lightbulb_state.temperature_min > lightbulb_state.temperature_max) {
    lightbulb_state.temperature_min = lightbulb_state.temperature_max;
  }
  if (lightbulb_state.temperature_default < lightbulb_state.temperature_min) {
    lightbulb_state.temperature_default = lightbulb_state.temperature_min;
  }
  if (lightbulb_state.temperature_default > lightbulb_state.temperature_max) {
    lightbulb_state.temperature_default = lightbulb_state.temperature_max;
  }
  if (lightbulb_state.temperature_current < lightbulb_state.temperature_min) {
    lightbulb_state.temperature_current = lightbulb_state.temperature_min;
  }
  if (lightbulb_state.temperature_current > lightbulb_state.temperature_max) {
    lightbulb_state.temperature_current = lightbulb_state.temperature_max;
  }
  if (lightbulb_state.temperature_target < lightbulb_state.temperature_min) {
    lightbulb_state.temperature_target = lightbulb_state.temperature_min;
  }
  if (lightbulb_state.temperature_target > lightbulb_state.temperature_max) {
    lightbulb_state.temperature_target = lightbulb_state.temperature_max;
  }
}

/***************************************************************************//**
 * This function loads the saved light state from Persistent Storage and
 * copies the data in the global variable lightbulb_state.
 * If PS key with ID 0x4004 does not exist or loading failed,
 * lightbulb_state is set to zero and some default values are written to it.
 *
 * @return 0 if loading succeeds. -1 if loading fails.
 ******************************************************************************/
static int lightbulb_state_load(void)
{
  struct gecko_msg_flash_ps_load_rsp_t* pLoad;

  pLoad = gecko_cmd_flash_ps_load(0x4004);

  // Set default values if ps_load fail or size of lightbulb_state has changed
  if (pLoad->result || (pLoad->value.len != sizeof(struct lightbulb_state))) {
    memset(&lightbulb_state, 0, sizeof(struct lightbulb_state));
    lightbulb_state.lightness_last = 0xFFFF;
    lightbulb_state.lightness_default = 0x0000;
    lightbulb_state.lightness_min = 0x0001;
    lightbulb_state.lightness_max = 0xFFFF;
    lightbulb_state.temperature_default = DEFAULT_TEMPERATURE;
    lightbulb_state.temperature_min = MIN_TEMPERATURE;
    lightbulb_state.temperature_max = MAX_TEMPERATURE;
    lightbulb_state.deltauv_default = DEFAULT_DELTAUV;

    /* Check if default values are valid and correct them if needed */
    lightbulb_state_validate_and_correct();
    return -1;
  }

  memcpy(&lightbulb_state, pLoad->value.data, pLoad->value.len);

  return 0;
}

/***************************************************************************//**
 * This function saves the current light state in Persistent Storage so that
 * the data is preserved over reboots and power cycles.
 * The light state is hold in a global variable lightbulb_state.
 * A PS key with ID 0x4004 is used to store the whole struct.
 *
 * @return 0 if saving succeed, -1 if saving fails.
 ******************************************************************************/
static int lightbulb_state_store(void)
{
  struct gecko_msg_flash_ps_save_rsp_t* pSave;

  pSave = gecko_cmd_flash_ps_save(0x4004, sizeof(struct lightbulb_state), (const uint8_t*)&lightbulb_state);

  if (pSave->result) {
    log("lightbulb_state_store(): PS save failed, code %x\r\n", pSave->result);
    return(-1);
  }

  return 0;
}

/***************************************************************************//**
 * This function is called each time the lightbulb state in RAM is changed.
 * It sets up a soft timer that will save the state in flash after small delay.
 * The purpose is to reduce amount of unnecessary flash writes.
 ******************************************************************************/
static void lightbulb_state_changed(void)
{
  gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(5000), TIMER_ID_SAVE_STATE, 1);
}

/*******************************************************************************
 * Lightbulb state initialization.
 * This is called at each boot if provisioning is already done.
 * Otherwise this function is called after provisioning is completed.
 ******************************************************************************/
void lightbulb_state_init(void)
{
  uint16_t res;

  /* Initialize mesh lib */
  mesh_lib_init(malloc, free, NUMBER_OF_MESH_LIB_MODELS);

  _primary_elem_index = 0;   // index of primary element is zero.
  _secondary_elem_index = 1; // index of secondary element is one.

  //Initialize Friend functionality
  log("Friend mode initialization\r\n");
  res = gecko_cmd_mesh_friend_init()->result;
  if (res) {
    log("Friend init failed 0x%x\r\n", res);
  }

  memset(&lightbulb_state, 0, sizeof(struct lightbulb_state));
  if (lightbulb_state_load() != 0) {
    log("lightbulb_state_load() failed, using defaults\r\n");
  }

  lc_init(_secondary_elem_index);
  scenes_init(_primary_elem_index);
  time_init(_primary_elem_index);
  scheduler_init(_primary_elem_index);

  // Handle on power up behavior
  uint32_t transition_ms = default_transition_time();
  switch (lightbulb_state.onpowerup) {
    case MESH_GENERIC_ON_POWER_UP_STATE_OFF:
      log("On power up state is OFF\r\n");
      lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_OFF;
      lightbulb_state.onoff_target = MESH_GENERIC_ON_OFF_STATE_OFF;
      lightbulb_state.lightness_current = 0;
      lightbulb_state.lightness_target = 0;
      LEDS_SetState(LED_STATE_OFF);
      lightbulb_state.temperature_current = lightbulb_state.temperature_default;
      lightbulb_state.temperature_target = lightbulb_state.temperature_default;
      lightbulb_state.deltauv_current = lightbulb_state.deltauv_default;
      lightbulb_state.deltauv_target = lightbulb_state.deltauv_default;
      LEDS_SetTemperature(lightbulb_state.temperature_default,
                          lightbulb_state.deltauv_default,
                          IMMEDIATE);
      break;

    case MESH_GENERIC_ON_POWER_UP_STATE_ON:
      log("On power up state is ON\r\n");
      lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      lightbulb_state.onoff_target = MESH_GENERIC_ON_OFF_STATE_ON;
      if (lightbulb_state.lightness_default == 0) {
        lightbulb_state.lightness_current = lightbulb_state.lightness_last;
        lightbulb_state.lightness_target = lightbulb_state.lightness_last;
      } else {
        lightbulb_state.lightness_current = lightbulb_state.lightness_default;
        lightbulb_state.lightness_target = lightbulb_state.lightness_default;
      }
      if (transition_ms > 0) {
        lightbulb_state.lightness_current = 0;
        LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
        gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                          TIMER_ID_LIGHTNESS_TRANSITION,
                                          1);
        LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);
      } else {
        LEDS_SetLevel(lightbulb_state.lightness_target, IMMEDIATE);
      }
      lightbulb_state.temperature_current = lightbulb_state.temperature_default;
      lightbulb_state.temperature_target = lightbulb_state.temperature_default;
      lightbulb_state.deltauv_current = lightbulb_state.deltauv_default;
      lightbulb_state.deltauv_target = lightbulb_state.deltauv_default;
      LEDS_SetTemperature(lightbulb_state.temperature_default,
                          lightbulb_state.deltauv_default,
                          IMMEDIATE);
      break;

    case MESH_GENERIC_ON_POWER_UP_STATE_RESTORE:
      log("On power up state is RESTORE\r\n");
      if (lc_get_mode() == 0) {
        if (transition_ms > 0 && lightbulb_state.lightness_target > 0) {
          lightbulb_state.lightness_current = 0;
          LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
          gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                            TIMER_ID_LIGHTNESS_TRANSITION,
                                            1);
          LEDS_SetLevel(lightbulb_state.lightness_target, transition_ms);
        } else {
          lightbulb_state.lightness_current = lightbulb_state.lightness_target;
          LEDS_SetLevel(lightbulb_state.lightness_current, IMMEDIATE);
        }

        if (lightbulb_state.lightness_current) {
          lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
        } else {
          lightbulb_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_OFF;
        }

        if (lightbulb_state.lightness_target) {
          lightbulb_state.onoff_target = MESH_GENERIC_ON_OFF_STATE_ON;
        } else {
          lightbulb_state.onoff_target = MESH_GENERIC_ON_OFF_STATE_OFF;
        }
      }

      if (transition_ms > 0
          && (lightbulb_state.temperature_target != lightbulb_state.temperature_default
              || lightbulb_state.deltauv_target != lightbulb_state.deltauv_default)) {
        lightbulb_state.temperature_current = lightbulb_state.temperature_default;
        lightbulb_state.deltauv_current = lightbulb_state.deltauv_default;
        LEDS_SetTemperature(lightbulb_state.temperature_current,
                            lightbulb_state.deltauv_current,
                            transition_ms);
        gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                          TIMER_ID_CTL_TEMP_TRANSITION,
                                          1);
        LEDS_SetTemperature(lightbulb_state.temperature_target,
                            lightbulb_state.deltauv_target,
                            transition_ms);
      } else {
        lightbulb_state.temperature_current = lightbulb_state.temperature_target;
        lightbulb_state.deltauv_current = lightbulb_state.deltauv_target;
        LEDS_SetTemperature(lightbulb_state.temperature_current,
                            lightbulb_state.deltauv_current,
                            IMMEDIATE);
      }
      break;

    default:
      break;
  }

  lightbulb_state_changed();
  init_models();
  lightness_setup_update(_primary_elem_index, mesh_lighting_state_lightness_default);
  lightness_setup_update(_primary_elem_index, mesh_lighting_state_lightness_range);
  ctl_setup_update(_primary_elem_index, mesh_lighting_state_ctl_default);
  ctl_setup_update(_primary_elem_index, mesh_lighting_state_ctl_temperature_range);
  lc_onpowerup_update(_secondary_elem_index, lightbulb_state.onpowerup);
  power_onoff_update_and_publish(_primary_elem_index);
  if (lc_get_mode() == 0) {
    onoff_update_and_publish(_primary_elem_index, IMMEDIATE);
    lightness_update_and_publish(_primary_elem_index, IMMEDIATE, mesh_lighting_state_lightness_actual);
  }
  ctl_temperature_update_and_publish(_secondary_elem_index, IMMEDIATE);
}

/** @} (end addtogroup lightbulb_state) */

/*******************************************************************************
 *  Handling of lightbulb timer events.
 *
 *  @param[in] evt  Pointer to incoming event.
 ******************************************************************************/
void handle_lightbulb_timer_evt(struct gecko_cmd_packet *evt)
{
  switch (evt->data.evt_hardware_soft_timer.handle) {
    case TIMER_ID_SAVE_STATE:
      /* save the lightbulb state */
      lightbulb_state_store();
      break;

    case TIMER_ID_DELAYED_ONOFF:
      /* delay for an on/off request has passed, now process the request */
      delayed_onoff_request();
      break;

    case TIMER_ID_DELAYED_LIGHTNESS:
      /* delay for a lightness request has passed, now process the request */
      delayed_lightness_request();
      break;

    case TIMER_ID_DELAYED_PRI_LEVEL:
      /* delay for a primary generic level request has passed, now process the request */
      delayed_pri_level_request();
      break;

    case TIMER_ID_DELAYED_CTL:
      /* delay for a ctl request has passed, now process the request */
      delayed_ctl_request();
      break;

    case TIMER_ID_DELAYED_CTL_TEMPERATURE:
      /* delay for a ctl temperature request has passed, now process the request */
      delayed_ctl_temperature_request();
      break;

    case TIMER_ID_DELAYED_SEC_LEVEL:
      /* delay for a secondary generic level request has passed, now process the request */
      delayed_sec_level_request();
      break;

    case TIMER_ID_ONOFF_TRANSITION:
      /* transition for an on/off request has completed, update the lightbulb state */
      onoff_transition_complete();
      break;

    case TIMER_ID_LIGHTNESS_TRANSITION:
      /* transition for a lightness request has completed, update the lightbulb state */
      lightness_transition_complete();
      break;

    case TIMER_ID_PRI_LEVEL_TRANSITION:
      /* transition for a primary generic level request has completed, update the lightbulb state */
      pri_level_transition_complete();
      break;

    case TIMER_ID_CTL_TRANSITION:
      /* transition for a ctl request has completed, update the lightbulb state */
      ctl_transition_complete();
      break;

    case TIMER_ID_CTL_TEMP_TRANSITION:
      /* transition for a ctl temperature request has completed, update the lightbulb state */
      ctl_temperature_transition_complete();
      break;

    case TIMER_ID_SEC_LEVEL_TRANSITION:
      /* transition for a secondary generic level request has completed, update the lightbulb state */
      sec_level_transition_complete();
      break;

    case TIMER_ID_PRI_LEVEL_MOVE:
      /* handling of generic level move, update the lightbulb state */
      pri_level_move_request();
      break;

    case TIMER_ID_SEC_LEVEL_MOVE:
      /* handling of generic level move, update the lightbulb state */
      sec_level_move_request();
      break;

    default:
      break;
  }
}

/** @} (end addtogroup Lightbulb) */
