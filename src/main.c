#include <zephyr/kernel.h>

#include "hardware_spec.h"
#include "cap_touch/cap_touch.h"
#include "io/led.h"
#include "bluetooth/bt_connection_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void _bt_event(enum bt_connection_state);
static void _cap_touch_event(uint8_t value);

int main(void) {
    /* simple blinking to indicate whether the system is working or not */
    led_init(LED_PIN, PORT_COMMON, LED_POLARITY);

    /* using Bluetooth to send captouch data */
    bt_connection_init(_bt_event);

    cap_touch_init(_cap_touch_event, CAPTOUCH_PSEL_COMP, CAPTOUCH_PSEL_PIN);

    cap_touch_start();
    led_blink();

    while(true) {
        LOG_INF("test\n");
        k_sleep(K_SECONDS(5));
        led_blink();
    }
    return 0;
}

static void _bt_event(enum bt_connection_state) {
    led_blink();
}

static void _cap_touch_event(uint8_t value) {
    led_blink();
}

