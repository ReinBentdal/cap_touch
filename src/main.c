#include <zephyr/kernel.h>

#include "hardware_spec.h"
#include "cap_touch/cap_touch.h"
#include "io/led.h"

#if CONFIG_DEBUG
#include "bluetooth/bt_connection_manager.h"
static void _bt_event(enum bt_connection_state);
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void _cap_touch_event(uint8_t value);

int main(void) {
    /* simple blinking to indicate whether the system is working or not */
    led_init(LED_PIN, PORT_COMMON, LED_POLARITY);

#if CONFIG_DEBUG
    /* using Bluetooth to send captouch data */
    bt_connection_init(_bt_event);
#endif

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

#if CONFIG_DEBUG
static void _bt_event(enum bt_connection_state) {
    led_blink();
}
#endif

static void _cap_touch_event(uint8_t value) {
    led_blink();
}