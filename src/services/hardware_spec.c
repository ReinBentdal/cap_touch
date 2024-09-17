#include "hardware_spec.h"

#include <zephyr/devicetree.h>

#include "nrf52.h"
#include "nrf52_bitfields.h"
#include <stddef.h>

const struct device* common_port = DEVICE_DT_GET(DT_NODELABEL(gpio0));

// values stored in UICR
#define HARDWARE_VERSION_WIMKY001_VALUE (0x80000001)
#define HARDWARE_VERSION_DK_VALUE (0xFFFFFFFF)

#ifdef DEBUG
#define DIS_HW_DK "dev kit [DEBUG]"
#define DIS_HW_WIMKY001 "WIMKY001 [DEBUG]"
#else
#define DIS_HW_DK "dev kit"
#define DIS_HW_WIMKY001 "WIMKY001"
#endif

static const struct hardware_spec _hw_dk = {
  .DIS_hw_rev_str = DIS_HW_DK,
  .pin_map = {-10, -10, -10, -6},
  .led_direction = 0,
  .cap_touch_psel = COMP_PSEL_PSEL_AnalogInput1,
};

static const struct hardware_spec _hw_wimky001 = {
  .DIS_hw_rev_str = DIS_HW_WIMKY001,
  .pin_map = {-4, -5, 5, 4, 6, 8, 7, 10, 9, 11, 17, 15, 13, 12, 20, 14, 16, 18, 19, 22, 21, 23, 24, -2, -10, -3, -1, 1, 0, 3, 2, -10},
  .led_direction = 0b00000000,
  .cap_touch_psel = COMP_PSEL_PSEL_AnalogInput7,
};

enum hardware_version hardware_version_get(void) {
  const uint32_t hardware_version_value = NRF_UICR->CUSTOMER[0];
  if (hardware_version_value == HARDWARE_VERSION_WIMKY001_VALUE) {
    return HARDWARE_VERSION_WIMKY001;
  }

  return HARDWARE_VERSION_DK;
}

const struct hardware_spec* hardware_spec_get(void) {
  switch (hardware_version_get()) {
    case HARDWARE_VERSION_WIMKY001:
      return &_hw_wimky001;
    default:
      return &_hw_dk;
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