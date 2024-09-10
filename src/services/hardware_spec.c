#include "hardware_spec.h"

#include "nrf52.h"
#include "nrf52_bitfields.h"
#include <stddef.h>

const struct device* common_port = DEVICE_DT_GET(DT_NODELABEL(gpio0));

// values stored in UICR
#define HARDWARE_VERSION_WIMKY001_VALUE (0x80000001)
#define HARDWARE_VERSION_REV2_VALUE (0xFFFFFFFF)

#ifdef DEBUG
#define DIS_HW_UNKNOWN "unknown [DEBUG]"
#define DIS_HW_REV2 "dev rev2 [DEBUG]"
#define DIS_HW_WIMKY001 "WIMKY001 [DEBUG]"
#else
#define DIS_HW_UNKNOWN "unknown"
#define DIS_HW_REV2 "dev rev2"
#define DIS_HW_WIMKY001 "WIMKY001"
#endif

static const struct hardware_spec _hw_unknown = {
  .DIS_hw_rev_str = DIS_HW_UNKNOWN,
  .pin_map = {0},
  .led_direction = 0,
  .cap_touch_psel = SAADC_CH_PSELP_PSELP_NC,
};

static const struct hardware_spec _hw_rev2 = {
  .DIS_hw_rev_str = DIS_HW_REV2,
  .pin_map = {5, 6, 7, 8, 9, 10, 11, -4, 20, 17, 15, 13, 12, 14, 16, 18, 19, 21, 23, 22, 24, -10, -10, -10, -10, -3, -1, 0, 1, 2, 3, 4},
  .led_direction = 0b00000011,
  .cap_touch_psel = SAADC_CH_PSELP_PSELP_NC,
};


static const struct hardware_spec _hw_wimky001 = {
  .DIS_hw_rev_str = DIS_HW_WIMKY001,
  .pin_map = {-4, -5, 5, 4, 6, 8, 7, 10, 9, 11, 17, 15, 13, 12, 20, 14, 16, 18, 19, 22, 21, 23, 24, -2, -10, -3, -1, 1, 0, 3, 2, -10},
  .led_direction = 0b00000000,
  .cap_touch_psel = SAADC_CH_PSELP_PSELP_AnalogInput7,
};

enum hardware_version hardware_version_get(void) {
  const uint32_t hardware_version_value = NRF_UICR->CUSTOMER[0];
  if (hardware_version_value == HARDWARE_VERSION_WIMKY001_VALUE) {
    return HARDWARE_VERSION_WIMKY001;
  } else if (hardware_version_value == HARDWARE_VERSION_REV2_VALUE) {
    return HARDWARE_VERSION_REV2;
  }

  return HARDWARE_VERSION_UNKNOWN;
}

const struct hardware_spec* hardware_spec_get(void) {
  switch (hardware_version_get()) {
    case HARDWARE_VERSION_WIMKY001:
      return &_hw_wimky001;
    case HARDWARE_VERSION_REV2:
      return &_hw_rev2;
    default:
      return &_hw_unknown;
    }
}

int hardware_spec_pin_get(int pin_identifer) {
  const struct hardware_spec* hw_spec = hardware_spec_get();
  for (int i = 0; i < 32; i++) {
    if (hw_spec->pin_map[i] == pin_identifer) {
      return i;
    }
  }
  return -1;
}