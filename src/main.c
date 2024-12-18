/*
 * File: main.c
 * Author: Rein Gundersen Bentdal
 * Created: 19.Des 2024
 *
 * Copyright (c) 2024, Rein Gundersen Bentdal
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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