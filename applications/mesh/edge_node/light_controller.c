/***************************************************************************//**
 * @file  light_controller.c
 * @brief LC module implementation
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
#include "light_controller.h"

/* C Standard Library headers */
#include <stdio.h>
#include <math.h>

/* Mesh specific headers */
#include "mesh_lib.h"
#include "mesh_device_properties.h"
#include "mesh_sensor.h"

/* LED driver with support for PWM dimming */
#include "led_driver.h"

#ifdef ENABLE_LOGGING
#define log(...) printf(__VA_ARGS__)
#else
#define log(...)
#endif

/***************************************************************************//**
 * @addtogroup LC
 * @{
 ******************************************************************************/

/** Timer Frequency used. */
#define TIMER_CLK_FREQ ((uint32)32768)
/** Convert msec to timer ticks. Maximum valid parameter is 65535999 ms. */
#define TIMER_MS_2_TIMERTICK(ms) ((uint32)((TIMER_CLK_FREQ * (uint64_t)ms) / 1000))
/** Immediate transition time is 0 seconds */
#define IMMEDIATE                           0
/** Value that represents unassigned element index */
#define UNASSIGNED_INDEX               0xffff

/** Key number for LC state */
#define LC_STATE_PSKEY                 0x4005
/** Key number for LC property state */
#define LC_PROPERTY_STATE_PSKEY        0x4006

/*******************************************************************************
 * Timer handles defines.
 ******************************************************************************/
#define TIMER_ID_SAVE_LC_STATE             61
#define TIMER_ID_SAVE_LC_PROPERTY_STATE    62
#define TIMER_ID_LC_ONOFF_TRANSITION       20
#define TIMER_ID_DELAYED_LC_ONOFF          21

/// LC state
static PACKSTRUCT(struct lc_state {
  uint8_t mode;                   /**< LC mode */
  uint8_t occupancy_mode;         /**< LC occupancy mode */
  uint8_t light_onoff;            /**< LC light onoff */
  uint8_t onoff_current;          /**< Current LC generic on/off value */
  uint8_t onoff_target;           /**< Target LC generic on/off value */
}) lc_state;

/// LC property state
static PACKSTRUCT(struct lc_property_state {
  light_control_time_occupancy_delay time_occupancy_delay: 24;    /**< Delay between receiving sensor occupancy message and changing the Light LC Occupancy state */
  light_control_time_fade_on time_fade_on: 24;                    /**< Transition time from a standby state to a run state */
  light_control_time_run_on time_run_on: 24;                      /**< Duration of the run state after last occupancy was detected */
  light_control_time_fade time_fade: 24;                          /**< Transition time from a run state to a prolong state */
  light_control_time_prolong time_prolong: 24;                    /**< Duration of the prolong state */
  light_control_time_standby_auto time_fade_standby_auto: 24;     /**< Transition time from a prolong state to a standby state when the transition is automatic */
  light_control_time_standby_manual time_fade_standby_manual: 24; /**< Transition time from a prolong state to a standby state when the transition is triggered by a manual operation */
  uint16_t lightness_on;                                          /**< Lightness level in a run state */
  uint16_t lightness_prolong;                                     /**< Lightness level in a prolong state */
  uint16_t lightness_standby;                                     /**< Lightness level in a standby state */
  illuminance_t ambient_luxlevel_on: 24;                          /**< Required Ambient LuxLevel level in the Run state */
  illuminance_t ambient_luxlevel_prolong: 24;                     /**< Required Ambient LuxLevel level in the Prolong state */
  illuminance_t ambient_luxlevel_standby: 24;                     /**< Required Ambient LuxLevel level in the Standby state */
  coefficient_t regulator_kiu;                                    /**< Integral coefficient of PI light regulator when increasing output */
  coefficient_t regulator_kid;                                    /**< Integral coefficient of PI light regulator when decreasing output */
  coefficient_t regulator_kpu;                                    /**< Proportional coefficient of PI light regulator when increasing output */
  coefficient_t regulator_kpd;                                    /**< Proportional coefficient of PI light regulator when decreasing output */
  percentage_8_t regulator_accuracy;                              /**< Accuracy of PI light regulator */
}) lc_property_state;

/// index of element where LC resides
static uint16_t lc_element = UNASSIGNED_INDEX;
/// copy of transition delay parameter, needed for delayed lc on/off request
static uint32_t delayed_lc_onoff_trans = 0;

static void lc_onoff_transition_complete(void);
static void delayed_lc_onoff_request(void);

/***************************************************************************//**
 * This function loads the saved light controller state from Persistent Storage
 * and copies the data in the global variable lc_state.
 * If PS key with ID 0x4005 does not exist or loading failed,
 * lc_state is set to zero and some default values are written to it.
 *
 * @return 0 if loading succeeds. -1 if loading fails.
 ******************************************************************************/
static int lc_state_load(void)
{
  struct gecko_msg_flash_ps_load_rsp_t* pLoad;

  pLoad = gecko_cmd_flash_ps_load(LC_STATE_PSKEY);

  // Set default values if ps_load fail or size of lc_state has changed
  if (pLoad->result || (pLoad->value.len != sizeof(lc_state))) {
    memset(&lc_state, 0, sizeof(lc_state));
    lc_state.mode = 0;
    lc_state.occupancy_mode = 0;
    lc_state.light_onoff = 0x00;
    return -1;
  }

  memcpy(&lc_state, pLoad->value.data, pLoad->value.len);

  return 0;
}

/***************************************************************************//**
 * This function saves the current light controller state in Persistent Storage
 * so that the data is preserved over reboots and power cycles.
 * The light controller state is hold in a global variable lc_state.
 * A PS key with ID 0x4005 is used to store the whole structure.
 *
 * @return 0 if saving succeed, -1 if saving fails.
 ******************************************************************************/
static int lc_state_store(void)
{
  struct gecko_msg_flash_ps_save_rsp_t* pSave;

  pSave = gecko_cmd_flash_ps_save(LC_STATE_PSKEY,
                                  sizeof(struct lc_state),
                                  (const uint8*)&lc_state);

  if (pSave->result) {
    log("lc_state_store(): PS save failed, code %x\r\n", pSave->result);
    return(-1);
  }

  return 0;
}

/***************************************************************************//**
 * This function is called each time the light controller state in RAM
 * is changed. It sets up a soft timer that will save the state in flash after
 * small delay. The purpose is to reduce amount of unnecessary flash writes.
 ******************************************************************************/
static void lc_state_changed(void)
{
  gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(5000),
                                    TIMER_ID_SAVE_LC_STATE,
                                    1);
}

/*******************************************************************************
 * This function is getter for current light controller mode.
 *
 * @return  current light controller mode
 ******************************************************************************/
uint8_t lc_get_mode(void)
{
  return lc_state.mode;
}

/*******************************************************************************
 * Light Controller state update on power up sequence.
 *
 * @param[in] element    Index of the element.
 * @param[in] onpowerup  Value of OnPowerUp state.
 ******************************************************************************/
void lc_onpowerup_update(uint16_t element, uint8_t onpowerup)
{
  switch (onpowerup) {
    case MESH_GENERIC_ON_POWER_UP_STATE_OFF:
    case MESH_GENERIC_ON_POWER_UP_STATE_ON:
      lc_state.mode = 0;
      lc_state.light_onoff = 0;
      lc_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_OFF;
      lc_state.onoff_target = MESH_GENERIC_ON_OFF_STATE_OFF;
      gecko_cmd_mesh_lc_server_update_mode(element, lc_state.mode);
      gecko_cmd_mesh_lc_server_update_om(element, lc_state.occupancy_mode);
      gecko_cmd_mesh_lc_server_update_light_onoff(element,
                                                  lc_state.light_onoff,
                                                  IMMEDIATE);
      break;

    case MESH_GENERIC_ON_POWER_UP_STATE_RESTORE:
      if (lc_state.mode == 0) {
        gecko_cmd_mesh_lc_server_update_mode(element, lc_state.mode);
        gecko_cmd_mesh_lc_server_update_om(element, lc_state.occupancy_mode);
      } else {
        gecko_cmd_mesh_lc_server_update_mode(element, lc_state.mode);
        gecko_cmd_mesh_lc_server_update_om(element, lc_state.occupancy_mode);
        if (lc_state.light_onoff == 0) {
          gecko_cmd_mesh_lc_server_update_light_onoff(element,
                                                      lc_state.light_onoff,
                                                      IMMEDIATE);
        } else {
          gecko_cmd_mesh_lc_server_update_light_onoff(element,
                                                      lc_state.light_onoff,
                                                      lc_property_state.time_fade_on);
        }
      }
      break;

    default:
      break;
  }

  lc_state_changed();
}

/***************************************************************************//**
 * This function loads the saved light controller property state from Persistent
 * Storage and copies the data in the global variable lc_property_state.
 * If PS key with ID 0x4006 does not exist or loading failed,
 * lc_property_state is set to zero and some default values are written to it.
 *
 * @return 0 if loading succeeds. -1 if loading fails.
 ******************************************************************************/
static int lc_property_state_load(void)
{
  struct gecko_msg_flash_ps_load_rsp_t* pLoad;

  pLoad = gecko_cmd_flash_ps_load(LC_PROPERTY_STATE_PSKEY);

  // Set default values if ps_load fail or size of lc_property_state has changed
  if (pLoad->result || (pLoad->value.len != sizeof(lc_property_state))) {
    memset(&lc_property_state, 0, sizeof(lc_property_state));
    lc_property_state.time_occupancy_delay = 0;
    lc_property_state.time_fade_on = 0;
    lc_property_state.time_run_on = 2000;
    lc_property_state.time_fade = 0;
    lc_property_state.time_prolong = 500;
    lc_property_state.time_fade_standby_auto = 0;
    lc_property_state.time_fade_standby_manual = 0;
    lc_property_state.lightness_on = 65535;
    lc_property_state.lightness_prolong = 32767;
    lc_property_state.lightness_standby = 2000;
    lc_property_state.ambient_luxlevel_on = 1000;
    lc_property_state.ambient_luxlevel_prolong = 500;
    lc_property_state.ambient_luxlevel_standby = 20;
    lc_property_state.regulator_kiu = 0.05;
    lc_property_state.regulator_kid = 0;
    lc_property_state.regulator_kpu = 0.4;
    lc_property_state.regulator_kpd = 0.3;
    lc_property_state.regulator_accuracy = 0xFF;
    return -1;
  }

  memcpy(&lc_property_state, pLoad->value.data, pLoad->value.len);

  return 0;
}

/***************************************************************************//**
 * This function saves the current light controller property state in Persistent
 * Storage so that the data is preserved over reboots and power cycles.
 * The light controller property state is hold in a global variable lc_property_state.
 * A PS key with ID 0x4006 is used to store the whole structure.
 *
 * @return 0 if saving succeed, -1 if saving fails.
 ******************************************************************************/
static int lc_property_state_store(void)
{
  struct gecko_msg_flash_ps_save_rsp_t* pSave;

  pSave = gecko_cmd_flash_ps_save(LC_PROPERTY_STATE_PSKEY,
                                  sizeof(struct lc_property_state),
                                  (const uint8*)&lc_property_state);

  if (pSave->result) {
    log("lc_property_state_store(): PS save failed, code %x\r\n", pSave->result);
    return(-1);
  }

  return 0;
}

/***************************************************************************//**
 * This function is called each time the light controller property state in RAM
 * is changed. It sets up a soft timer that will save the state in flash after
 * small delay. The purpose is to reduce amount of unnecessary flash writes.
 ******************************************************************************/
static void lc_property_state_changed(void)
{
  gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(5000),
                                    TIMER_ID_SAVE_LC_PROPERTY_STATE,
                                    1);
}

/***************************************************************************//**
 * This function update property in stack based on property data.
 *
 * @param[in] element         Index of the element.
 * @param[in] pProperty_data  Pointer to property data array that contains:
 *                              - property ID in first two bytes,
 *                              - length of data in third byte,
 *                              - property value in the next bytes.
 ******************************************************************************/
static void update_property(uint16_t element, const uint8_t *pProperty_data)
{
  uint16_t property_id = (uint16_t)pProperty_data[0]
                         | ((uint16_t)pProperty_data[1] << 8);
  uint16_t result =
    gecko_cmd_mesh_lc_setup_server_update_property(element,
                                                   property_id,
                                                   pProperty_data[2],
                                                   &pProperty_data[3])->result;
  if (result) {
    log("lc_setup_server_update_property failed, error=%u\r\n", result);
  }
}

/***************************************************************************//**
 * This function update Light LC Time Occupancy Delay property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_occupancy_delay_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_occupancy_delay delay = lc_property_state.time_occupancy_delay;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_OCCUPANCY_DELAY,
                          property_data,
                          (uint8_t*)&delay);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Time Fade On property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_fade_on_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_fade_on fade_on = lc_property_state.time_fade_on;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_FADE_ON,
                          property_data,
                          (uint8_t*)&fade_on);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Time Run On property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_run_on_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_run_on run_on = lc_property_state.time_run_on;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_RUN_ON,
                          property_data,
                          (uint8_t*)&run_on);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Time Fade property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_fade_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_fade fade = lc_property_state.time_fade;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_FADE,
                          property_data,
                          (uint8_t*)&fade);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Time Prolong property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_prolong_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_prolong prolong = lc_property_state.time_prolong;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_PROLONG,
                          property_data,
                          (uint8_t*)&prolong);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Time Fade Standby Auto property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_fade_standby_auto_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_standby_auto standby_auto = lc_property_state.time_fade_standby_auto;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_FADE_STANDBY_AUTO,
                          property_data,
                          (uint8_t*)&standby_auto);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Time Fade Standby Manual property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_time_fade_standby_manual_update(uint16_t element)
{
  uint8_t property_data[6];
  light_control_time_standby_manual standby_manual = lc_property_state.time_fade_standby_manual;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_TIME_FADE_STANDBY_MANUAL,
                          property_data,
                          (uint8_t*)&standby_manual);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Lightness On property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_lightness_on_update(uint16_t element)
{
  uint8_t property_data[5];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_LIGHTNESS_ON,
                          property_data,
                          (uint8_t*)&lc_property_state.lightness_on);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Lightness Prolong property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_lightness_prolong_update(uint16_t element)
{
  uint8_t property_data[5];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_LIGHTNESS_PROLONG,
                          property_data,
                          (uint8_t*)&lc_property_state.lightness_prolong);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Lightness Standby property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_lightness_standby_update(uint16_t element)
{
  uint8_t property_data[5];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_LIGHTNESS_STANDBY,
                          property_data,
                          (uint8_t*)&lc_property_state.lightness_standby);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Ambient LuxLevel On property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_ambient_luxlevel_on_update(uint16_t element)
{
  uint8_t property_data[6];
  illuminance_t ambient_luxlevel_on = lc_property_state.ambient_luxlevel_on;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_AMBIENT_LUXLEVEL_ON,
                          property_data,
                          (uint8_t*)&ambient_luxlevel_on);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Ambient LuxLevel Prolong property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_ambient_luxlevel_prolong_update(uint16_t element)
{
  uint8_t property_data[6];
  illuminance_t ambient_luxlevel_prolong = lc_property_state.ambient_luxlevel_prolong;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_AMBIENT_LUXLEVEL_PROLONG,
                          property_data,
                          (uint8_t*)&ambient_luxlevel_prolong);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Ambient LuxLevel Standby property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_ambient_luxlevel_standby_update(uint16_t element)
{
  uint8_t property_data[6];
  illuminance_t ambient_luxlevel_standby = lc_property_state.ambient_luxlevel_standby;
  mesh_sensor_data_to_buf(LIGHT_CONTROL_AMBIENT_LUXLEVEL_STANDBY,
                          property_data,
                          (uint8_t*)&ambient_luxlevel_standby);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Regulator Kiu property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_regulator_kiu_update(uint16_t element)
{
  uint8_t property_data[7];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_REGULATOR_KIU,
                          property_data,
                          (uint8_t*)&lc_property_state.regulator_kiu);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Regulator Kid property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_regulator_kid_update(uint16_t element)
{
  uint8_t property_data[7];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_REGULATOR_KID,
                          property_data,
                          (uint8_t*)&lc_property_state.regulator_kid);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Regulator Kpu property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_regulator_kpu_update(uint16_t element)
{
  uint8_t property_data[7];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_REGULATOR_KPU,
                          property_data,
                          (uint8_t*)&lc_property_state.regulator_kpu);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Regulator Kpd property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_regulator_kpd_update(uint16_t element)
{
  uint8_t property_data[7];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_REGULATOR_KPD,
                          property_data,
                          (uint8_t*)&lc_property_state.regulator_kpd);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update Light LC Regulator Accuracy property in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_regulator_accuracy_update(uint16_t element)
{
  uint8_t property_data[4];
  mesh_sensor_data_to_buf(LIGHT_CONTROL_REGULATOR_ACCURACY,
                          property_data,
                          (uint8_t*)&lc_property_state.regulator_accuracy);
  update_property(element, property_data);
}

/***************************************************************************//**
 * This function update all light controller properties in stack.
 *
 * @param[in] element  Index of the element.
 ******************************************************************************/
static void lc_property_state_update(uint16_t element)
{
  lc_time_occupancy_delay_update(element);
  lc_time_fade_on_update(element);
  lc_time_run_on_update(element);
  lc_time_fade_update(element);
  lc_time_prolong_update(element);
  lc_time_fade_standby_auto_update(element);
  lc_time_fade_standby_manual_update(element);
  lc_lightness_on_update(element);
  lc_lightness_prolong_update(element);
  lc_lightness_standby_update(element);
  lc_ambient_luxlevel_on_update(element);
  lc_ambient_luxlevel_prolong_update(element);
  lc_ambient_luxlevel_standby_update(element);
  lc_regulator_kiu_update(element);
  lc_regulator_kid_update(element);
  lc_regulator_kpu_update(element);
  lc_regulator_kpd_update(element);
  lc_regulator_accuracy_update(element);
}

/*******************************************************************************
 * LC initialization.
 * This should be called at each boot if provisioning is already done.
 * Otherwise this function should be called after provisioning is completed.
 *
 * @param[in] element  Index of the element where LC model is initialized.
 *
 * @return Status of the initialization operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
uint16_t lc_init(uint16_t element)
{
  // Initialize lc server models
  uint16_t result = gecko_cmd_mesh_lc_server_init(element)->result;
  if (result) {
    log("mesh_lc_server_init failed, code 0x%x\r\n", result);
  }

  lc_element = element;

  memset(&lc_state, 0, sizeof(lc_state));
  if (lc_state_load() != 0) {
    log("lc_state_load() failed, using defaults\r\n");
  }

  memset(&lc_property_state, 0, sizeof(lc_property_state));
  if (lc_property_state_load() != 0) {
    log("lc_property_state_load() failed, using defaults\r\n");
  }

  // Set the regulator interval to 100 milliseconds. If you want to use shorter
  // intervals, you should disable some logs in order not to affect performance.
  result = gecko_cmd_mesh_lc_server_set_regulator_interval(element, 100)->result;
  if (result) {
    log("mesh_lc_server_set_regulator_interval failed, code 0x%x\r\n", result);
  }

  lc_property_state_update(element);
  lc_property_state_changed();

  return result;
}

/***************************************************************************//**
 * Handling of lc server mode updated event.
 *
 * @param[in] pEvt  Pointer to lc server mode updated event.
 ******************************************************************************/
static void handle_lc_server_mode_updated_event(
  struct gecko_msg_mesh_lc_server_mode_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_server_mode_updated_id, mode=%u\r\n",
      pEvt->mode_value);
  lc_state.mode = pEvt->mode_value;
  lc_state_changed();
}

/***************************************************************************//**
 * Handling of lc server occupancy mode updated event.
 *
 * @param[in] pEvt  Pointer to lc server occupancy mode updated event.
 ******************************************************************************/
static void handle_lc_server_om_updated_event(
  struct gecko_msg_mesh_lc_server_om_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_server_om_updated_id, om=%u\r\n",
      pEvt->om_value);
  lc_state.occupancy_mode = pEvt->om_value;
  lc_state_changed();
}

/***************************************************************************//**
 * Handling of lc server light onoff updated event.
 *
 * @param[in] pEvt  Pointer to lc server light onoff updated event.
 ******************************************************************************/
static void handle_lc_server_light_onoff_updated_event(
  struct gecko_msg_mesh_lc_server_light_onoff_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_server_light_onoff_updated_id, lc_onoff=%u, transtime=%lu\r\n",
      pEvt->onoff_state,
      pEvt->onoff_trans_time);
  lc_state.light_onoff = pEvt->onoff_state;
  lc_state_changed();
}

/***************************************************************************//**
 * Handling of lc server occupancy updated event.
 *
 * @param[in] pEvt  Pointer to lc server occupancy updated event.
 ******************************************************************************/
static void handle_lc_server_occupancy_updated_event(
  struct gecko_msg_mesh_lc_server_occupancy_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_server_occupancy_updated_id, occupancy=%u\r\n",
      pEvt->occupancy_value);
}

/***************************************************************************//**
 * Handling of lc server ambient lux level updated event.
 *
 * @param[in] pEvt  Pointer to lc server ambient lux level updated event.
 ******************************************************************************/
static void handle_lc_server_ambient_lux_level_updated_event(
  struct gecko_msg_mesh_lc_server_ambient_lux_level_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_server_ambient_lux_level_updated_id, lux_level=%lu\r\n",
      pEvt->ambient_lux_level_value);
}

/***************************************************************************//**
 * Handling of lc server linear output updated event.
 *
 * @param[in] pEvt  Pointer to lc server linear output updated event.
 ******************************************************************************/
static void handle_lc_server_linear_output_updated_event(
  struct gecko_msg_mesh_lc_server_linear_output_updated_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_server_linear_output_updated_id, linear_output=%u\r\n",
      pEvt->linear_output_value);
  // Convert from linear to actual lightness value
  uint32_t lightness = (uint32_t)sqrt(65535 * (uint32_t)(pEvt->linear_output_value));
  // Update LED
  LEDS_SetLevel(lightness, IMMEDIATE);
}

/***************************************************************************//**
 * Printing the float number using integers.
 *
 * @param[in] number  Number to print.
 ******************************************************************************/
static void print_float(float number)
{
  if (number > INT32_MAX) {
    log("> %ld", INT32_MAX);
  } else if (number < INT32_MIN) {
    log("< %ld", INT32_MIN);
  } else {
    uint16_t fraction = (number > 0 ? number - (int32_t)number : (int32_t)number - number) * 1000;
    log("%ld.%03u",
        (int32_t)number,
        fraction);
  }
}

/***************************************************************************//**
 * Handling of lc setup server set property event.
 *
 * @param[in] pEvt  Pointer to lc setup server set property event.
 ******************************************************************************/
static void handle_lc_setup_server_set_property(
  struct gecko_msg_mesh_lc_setup_server_set_property_evt_t *pEvt)
{
  log("evt:gecko_evt_mesh_lc_setup_server_property_set_id, property=0x%4.4x, value=0x",
      pEvt->property_id);
  for (int i = 0; i < pEvt->property_value.len; i++) {
    log("%2.2x", pEvt->property_value.data[i]);
  }
  log("\r\n");

  switch (pEvt->property_id) {
    case LIGHT_CONTROL_TIME_OCCUPANCY_DELAY:
      lc_property_state.time_occupancy_delay =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_OCCUPANCY_DELAY,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Occupancy Delay = %u.%03us\r\n",
          lc_property_state.time_occupancy_delay / 1000,
          lc_property_state.time_occupancy_delay % 1000);
      break;

    case LIGHT_CONTROL_TIME_FADE_ON:
      lc_property_state.time_fade_on =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_FADE_ON,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Fade On = %u.%03us\r\n",
          lc_property_state.time_fade_on / 1000,
          lc_property_state.time_fade_on % 1000);
      break;

    case LIGHT_CONTROL_TIME_RUN_ON:
      lc_property_state.time_run_on =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_RUN_ON,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Run On = %u.%03us\r\n",
          lc_property_state.time_run_on / 1000,
          lc_property_state.time_run_on % 1000);
      break;

    case LIGHT_CONTROL_TIME_FADE:
      lc_property_state.time_fade =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_FADE,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Fade = %u.%03us\r\n",
          lc_property_state.time_fade / 1000,
          lc_property_state.time_fade % 1000);
      break;

    case LIGHT_CONTROL_TIME_PROLONG:
      lc_property_state.time_prolong =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_PROLONG,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Prolong = %u.%03us\r\n",
          lc_property_state.time_prolong / 1000,
          lc_property_state.time_prolong % 1000);
      break;

    case LIGHT_CONTROL_TIME_FADE_STANDBY_AUTO:
      lc_property_state.time_fade_standby_auto =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_FADE_STANDBY_AUTO,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Fade Standby Auto = %u.%03us\r\n",
          lc_property_state.time_fade_standby_auto / 1000,
          lc_property_state.time_fade_standby_auto % 1000);
      break;

    case LIGHT_CONTROL_TIME_FADE_STANDBY_MANUAL:
      lc_property_state.time_fade_standby_manual =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_TIME_FADE_STANDBY_MANUAL,
                                  pEvt->property_value.data)
        .time_millisecond_24;
      log("Light Control Time Fade Standby Manual = %u.%03us\r\n",
          lc_property_state.time_fade_standby_manual / 1000,
          lc_property_state.time_fade_standby_manual % 1000);
      break;

    case LIGHT_CONTROL_LIGHTNESS_ON:
      lc_property_state.lightness_on =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_LIGHTNESS_ON,
                                  pEvt->property_value.data)
        .uint16;
      log("Light Control Lightness On = %u\r\n",
          lc_property_state.lightness_on);
      break;

    case LIGHT_CONTROL_LIGHTNESS_PROLONG:
      lc_property_state.lightness_prolong =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_LIGHTNESS_PROLONG,
                                  pEvt->property_value.data)
        .uint16;
      log("Light Control Lightness Prolong = %u\r\n",
          lc_property_state.lightness_prolong);
      break;

    case LIGHT_CONTROL_LIGHTNESS_STANDBY:
      lc_property_state.lightness_standby =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_LIGHTNESS_STANDBY,
                                  pEvt->property_value.data)
        .uint16;
      log("Light Control Lightness Standby = %u\r\n",
          lc_property_state.lightness_standby);
      break;

    case LIGHT_CONTROL_AMBIENT_LUXLEVEL_ON:
      lc_property_state.ambient_luxlevel_on =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_AMBIENT_LUXLEVEL_ON,
                                  pEvt->property_value.data)
        .illuminance;
      log("Light Control Ambient LuxLevel On = %u.%02ulux\r\n",
          lc_property_state.ambient_luxlevel_on / 100,
          lc_property_state.ambient_luxlevel_on % 100);
      break;

    case LIGHT_CONTROL_AMBIENT_LUXLEVEL_PROLONG:
      lc_property_state.ambient_luxlevel_prolong =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_AMBIENT_LUXLEVEL_PROLONG,
                                  pEvt->property_value.data)
        .illuminance;
      log("Light Control Ambient LuxLevel Prolong = %u.%02ulux\r\n",
          lc_property_state.ambient_luxlevel_prolong / 100,
          lc_property_state.ambient_luxlevel_prolong % 100);
      break;

    case LIGHT_CONTROL_AMBIENT_LUXLEVEL_STANDBY:
      lc_property_state.ambient_luxlevel_standby =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_AMBIENT_LUXLEVEL_STANDBY,
                                  pEvt->property_value.data)
        .illuminance;
      log("Light Control Ambient LuxLevel Standby = %u.%02ulux\r\n",
          lc_property_state.ambient_luxlevel_standby / 100,
          lc_property_state.ambient_luxlevel_standby % 100);
      break;

    case LIGHT_CONTROL_REGULATOR_KIU:
      lc_property_state.regulator_kiu =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_REGULATOR_KIU,
                                  pEvt->property_value.data)
        .coefficient;
      log("Light Control Regulator Kiu = ");
      print_float(lc_property_state.regulator_kiu);
      log("\r\n");
      break;

    case LIGHT_CONTROL_REGULATOR_KID:
      lc_property_state.regulator_kid =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_REGULATOR_KID,
                                  pEvt->property_value.data)
        .coefficient;
      log("Light Control Regulator Kid = ");
      print_float(lc_property_state.regulator_kid);
      log("\r\n");
      break;

    case LIGHT_CONTROL_REGULATOR_KPU:
      lc_property_state.regulator_kpu =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_REGULATOR_KPU,
                                  pEvt->property_value.data)
        .coefficient;
      log("Light Control Regulator Kpu = ");
      print_float(lc_property_state.regulator_kpu);
      log("\r\n");
      break;

    case LIGHT_CONTROL_REGULATOR_KPD:
      lc_property_state.regulator_kpd =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_REGULATOR_KPD,
                                  pEvt->property_value.data)
        .coefficient;
      log("Light Control Regulator Kpd = ");
      print_float(lc_property_state.regulator_kpd);
      log("\r\n");
      break;

    case LIGHT_CONTROL_REGULATOR_ACCURACY:
      lc_property_state.regulator_accuracy =
        mesh_sensor_data_from_buf(LIGHT_CONTROL_REGULATOR_ACCURACY,
                                  pEvt->property_value.data)
        .percentage;
      if (lc_property_state.regulator_accuracy == 0xFF) {
        log("Light Control Regulator Accuracy = Value is not known\r\n");
      } else {
        log("Light Control Regulator Accuracy = %u.%u%%\r\n",
            lc_property_state.regulator_accuracy / 2,
            (lc_property_state.regulator_accuracy % 2) * 5);
      }
      break;

    default:
      break;
  }

  lc_property_state_changed();
}

/*******************************************************************************
 *  Handling of mesh light controller events.
 *  It handles:
 *   - lc_server_mode_updated
 *   - lc_server_om_updated
 *   - lc_server_light_onoff_updated
 *   - lc_server_occupancy_updated
 *   - lc_server_ambient_lux_level_updated
 *   - lc_server_linear_output_updated
 *   - lc_setup_server_set_property
 *
 *  @param[in] pEvt  Pointer to incoming light controller event.
 ******************************************************************************/
void handle_lc_server_events(struct gecko_cmd_packet *pEvt)
{
  switch (BGLIB_MSG_ID(pEvt->header)) {
    case gecko_evt_mesh_lc_server_mode_updated_id:
      handle_lc_server_mode_updated_event(
        &(pEvt->data.evt_mesh_lc_server_mode_updated));
      break;

    case gecko_evt_mesh_lc_server_om_updated_id:
      handle_lc_server_om_updated_event(
        &(pEvt->data.evt_mesh_lc_server_om_updated));
      break;

    case gecko_evt_mesh_lc_server_light_onoff_updated_id:
      handle_lc_server_light_onoff_updated_event(
        &(pEvt->data.evt_mesh_lc_server_light_onoff_updated));
      break;

    case gecko_evt_mesh_lc_server_occupancy_updated_id:
      handle_lc_server_occupancy_updated_event(
        &(pEvt->data.evt_mesh_lc_server_occupancy_updated));
      break;

    case gecko_evt_mesh_lc_server_ambient_lux_level_updated_id:
      handle_lc_server_ambient_lux_level_updated_event(
        &(pEvt->data.evt_mesh_lc_server_ambient_lux_level_updated));
      break;

    case gecko_evt_mesh_lc_server_linear_output_updated_id:
      handle_lc_server_linear_output_updated_event(
        &(pEvt->data.evt_mesh_lc_server_linear_output_updated));
      break;

    case gecko_evt_mesh_lc_setup_server_set_property_id:
      handle_lc_setup_server_set_property(
        &(pEvt->data.evt_mesh_lc_setup_server_set_property));
      break;

    default:
      break;
  }
}

/*******************************************************************************
 * Handling of light controller timer events.
 *
 * @param[in] evt  Pointer to incoming event.
 ******************************************************************************/
void handle_lc_timer_evt(struct gecko_cmd_packet *evt)
{
  switch (evt->data.evt_hardware_soft_timer.handle) {
    case TIMER_ID_SAVE_LC_STATE:
      /* save the light controller state */
      lc_state_store();
      break;

    case TIMER_ID_SAVE_LC_PROPERTY_STATE:
      /* save the light controller property state */
      lc_property_state_store();
      break;

    case TIMER_ID_DELAYED_LC_ONOFF:
      /* delay for lc on/off request has passed, now process the request */
      delayed_lc_onoff_request();
      break;

    case TIMER_ID_LC_ONOFF_TRANSITION:
      /* transition for lc on/off request has completed, update the lc state */
      lc_onoff_transition_complete();
      break;

    default:
      break;
  }
}

/***************************************************************************//**
 * @addtogroup LC_GenericOnOff
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * Response to LC generic on/off request.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] client_addr    Address of the client model which sent the message.
 * @param[in] appkey_index   The application key index used in encrypting.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the response operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lc_onoff_response(uint16_t element_index,
                                     uint16_t client_addr,
                                     uint16_t appkey_index,
                                     uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_on_off;
  current.on_off.on = lc_state.onoff_current;

  target.kind = mesh_generic_state_on_off;
  target.on_off.on = lc_state.onoff_target;

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
 * Update LC generic on/off state.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lc_onoff_update(uint16_t element_index, uint32_t remaining_ms)
{
  struct mesh_generic_state current, target;

  current.kind = mesh_generic_state_on_off;
  current.on_off.on = lc_state.onoff_current;

  target.kind = mesh_generic_state_on_off;
  target.on_off.on = lc_state.onoff_target;

  return mesh_lib_generic_server_update(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                        element_index,
                                        &current,
                                        &target,
                                        remaining_ms);
}

/***************************************************************************//**
 * Update LC generic on/off state and publish model state to the network.
 *
 * @param[in] element_index  Server model element index.
 * @param[in] remaining_ms   The remaining time in milliseconds.
 *
 * @return Status of the update and publish operation.
 *         Returns bg_err_success (0) if succeed, non-zero otherwise.
 ******************************************************************************/
static errorcode_t lc_onoff_update_and_publish(uint16_t element_index,
                                               uint32_t remaining_ms)
{
  errorcode_t e;

  e = lc_onoff_update(element_index, remaining_ms);
  if (e == bg_err_success) {
    e = mesh_lib_generic_server_publish(MESH_GENERIC_ON_OFF_SERVER_MODEL_ID,
                                        element_index,
                                        mesh_generic_state_on_off);
  }

  return e;
}

/*******************************************************************************
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
                      uint8_t request_flags)
{
  log("LC ON/OFF request: requested state=<%s>, transition=%lu, delay=%u\r\n",
      request->on_off ? "ON" : "OFF", transition_ms, delay_ms);

  if (lc_state.onoff_current == request->on_off) {
    log("Request for current state received; no op\r\n");
  } else {
    log("Turning lc light <%s>\r\n", request->on_off ? "ON" : "OFF");
    if (transition_ms == 0 && delay_ms == 0) { // Immediate change
      lc_state.onoff_current = request->on_off;
      lc_state.onoff_target = request->on_off;
    } else if (delay_ms > 0) {
      // a delay has been specified for the lc light change. Start a soft timer
      // that will trigger the change after the given delay
      // Current state remains as is for now
      lc_state.onoff_target = request->on_off;
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delay_ms),
                                        TIMER_ID_DELAYED_LC_ONOFF,
                                        1);
      // store transition parameter for later use
      delayed_lc_onoff_trans = transition_ms;
    } else {
      // no delay but transition time has been set.
      lc_state.onoff_target = request->on_off;
      if (lc_state.onoff_target == MESH_GENERIC_ON_OFF_STATE_ON) {
        lc_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      }
      lc_onoff_update(element_index, transition_ms);

      // lc current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_LC_ONOFF_TRANSITION,
                                        1);
    }
    lc_state_changed();
    // State has changed, so the current scene number is reset
    gecko_cmd_mesh_scene_server_reset_register(element_index);
  }

  uint32_t remaining_ms = delay_ms + transition_ms;
  if (request_flags & MESH_REQUEST_FLAG_RESPONSE_REQUIRED) {
    lc_onoff_response(element_index, client_addr, appkey_index, remaining_ms);
  }
  lc_onoff_update_and_publish(element_index, remaining_ms);
}

/*******************************************************************************
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
                     uint32_t remaining_ms)
{
  if (current->on_off.on != lc_state.onoff_current) {
    log("LC on-off state changed %u to %u\r\n", lc_state.onoff_current, current->on_off.on);

    lc_state.onoff_current = current->on_off.on;
    lc_state_changed();
  } else {
    log("dummy LC onoff change - same state as before\r\n");
  }
}

/*******************************************************************************
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
                     uint32_t transition_ms)
{
  log("LC Generic On/Off recall\r\n");
  if (transition_ms == IMMEDIATE) {
    lc_state.onoff_target = current->on_off.on;
  } else {
    lc_state.onoff_target = target->on_off.on;
  }

  if (lc_state.onoff_current == lc_state.onoff_target) {
    log("Request for current state received; no op\r\n");
  } else {
    log("recall ON/OFF state <%s> with transition=%lu ms\r\n",
        lc_state.onoff_target ? "ON" : "OFF",
        transition_ms);

    if (transition_ms == IMMEDIATE) {
      lc_state.onoff_current = current->on_off.on;
    } else {
      if (lc_state.onoff_target == MESH_GENERIC_ON_OFF_STATE_ON) {
        lc_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      }
      // lc current state will be updated when transition is complete
      gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(transition_ms),
                                        TIMER_ID_LC_ONOFF_TRANSITION,
                                        1);
    }
    lc_state_changed();
  }

  lc_onoff_update_and_publish(element_index, transition_ms);
}

/***************************************************************************//**
 * This function is called when a LC on/off request
 * with non-zero transition time has completed.
 ******************************************************************************/
static void lc_onoff_transition_complete(void)
{
  // transition done -> set state, update and publish
  lc_state.onoff_current = lc_state.onoff_target;

  log("transition complete. New state is %s\r\n", lc_state.onoff_current ? "ON" : "OFF");

  lc_state_changed();
  lc_onoff_update_and_publish(lc_element, IMMEDIATE);
}

/***************************************************************************//**
 * This function is called when delay for LC on/off request has completed.
 ******************************************************************************/
static void delayed_lc_onoff_request(void)
{
  log("starting delayed LC on/off request: %u -> %u, %lu ms\r\n",
      lc_state.onoff_current,
      lc_state.onoff_target,
      delayed_lc_onoff_trans
      );

  if (delayed_lc_onoff_trans == 0) {
    // no transition delay, update state immediately

    lc_state.onoff_current = lc_state.onoff_target;
    lc_state_changed();
    lc_onoff_update_and_publish(lc_element, delayed_lc_onoff_trans);
  } else {
    if (lc_state.onoff_target == MESH_GENERIC_ON_OFF_STATE_ON) {
      lc_state.onoff_current = MESH_GENERIC_ON_OFF_STATE_ON;
      lc_onoff_update(lc_element, delayed_lc_onoff_trans);
    }

    // state is updated when transition is complete
    gecko_cmd_hardware_set_soft_timer(TIMER_MS_2_TIMERTICK(delayed_lc_onoff_trans),
                                      TIMER_ID_LC_ONOFF_TRANSITION,
                                      1);
  }
}

/** @} (end addtogroup LC_GenericOnOff) */
/** @} (end addtogroup LC) */
