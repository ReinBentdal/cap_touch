#ifndef _HARDWARE_SPEC_H_
#define _HARDWARE_SPEC_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

/* positive values are the keys and negative are others */
#define HARDWARE_SPEC_PIN_UNUSED (-10)
#define HARDWARE_SPEC_PIN_SHIFT (-1)
#define HARDWARE_SPEC_PIN_LOCK (-2)
#define HARDWARE_SPEC_PIN_LED_TOWER (-3)
#define HARDWARE_SPEC_PIN_LED_MONKEY0 (-4)
#define HARDWARE_SPEC_PIN_LED_MONKEY1 (-5)
#define HARDWARE_SPEC_PIN_CAPTOUCH (-6)

enum hardware_version {
    HARDWARE_VERSION_DK = 0,
    HARDWARE_VERSION_WIMKY001 = 1,
};

struct hardware_spec {
    int8_t pin_map[32];
    char* DIS_hw_rev_str;
    uint8_t led_direction;
    uint8_t cap_touch_psel;
};

extern const struct device* common_port;

enum hardware_version hardware_version_get(void);
const struct hardware_spec* hardware_spec_get(void);

int hardware_spec_pin_get(int pin_identifer);

#endif