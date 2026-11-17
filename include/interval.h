#ifndef INTERVAL_H
#define INTERVAL_H

#include "FurbleSpinValue.h"

namespace Furble {

constexpr SpinValue::nvs_t INTERVAL_DEFAULT_WAIT = {0, SpinValue::UNIT_SEC};
constexpr SpinValue::nvs_t INTERVAL_DEFAULT_COUNT = {10, SpinValue::UNIT_NIL};
constexpr SpinValue::nvs_t INTERVAL_DEFAULT_DELAY = {15, SpinValue::UNIT_SEC};

/**
 * The original shutter default.
 *
 * 30ms is too short for the camera to acquire focus and start the shutter,
 * existing settings using it are migrated on load.
 */
constexpr SpinValue::nvs_t INTERVAL_LEGACY_SHUTTER = {30, SpinValue::UNIT_MS};

/**
 * Default time the shutter is held down for each intervalometer capture.
 *
 * Long enough for the camera to focus and fire, see #208.
 */
constexpr SpinValue::nvs_t INTERVAL_DEFAULT_SHUTTER = {400, SpinValue::UNIT_MS};

/**
 * Version 1 of the NVS interval setting.
 */
typedef struct __attribute__((packed)) {
  SpinValue::nvs_t count;
  SpinValue::nvs_t delay;
  SpinValue::nvs_t shutter;
} interval_v1_t;

/**
 * Current version of the NVS interval setting.
 */
typedef struct __attribute__((packed)) {
  SpinValue::nvs_t count;
  SpinValue::nvs_t delay;
  SpinValue::nvs_t shutter;
  SpinValue::nvs_t wait;
} interval_t;

}  // namespace Furble

#endif
