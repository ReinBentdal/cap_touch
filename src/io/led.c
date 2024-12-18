/*
 * File: led.c
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

#include "led.h"

#include <zephyr/kernel.h>

static volatile NRF_GPIO_Type* _port;
static uint32_t _pin;
static int _polarity;

static void _led_set_state(int state);
static void _blink_done(struct k_work *);
static K_WORK_DELAYABLE_DEFINE(_blink_done_work, _blink_done);

void led_init(uint32_t pin, volatile NRF_GPIO_Type* port, int polarity) {
    __ASSERT_NO_MSG(port != NULL);
    __ASSERT_NO_MSG(pin < 32);
    __ASSERT_NO_MSG(polarity == 0 || polarity == 1);

    _port = port;
    _pin = pin;
    _polarity = polarity;

    _led_set_state(0);

    _port->DIRSET = 1 << _pin;
}

void led_blink(void) {
    _led_set_state(1);
    k_work_reschedule(&_blink_done_work, K_MSEC(20));
}

static void _blink_done(struct k_work *work) {
    _led_set_state(0);
}

static void _led_set_state(int state) {
    if (state ^ _polarity) {
        _port->OUTCLR = 1 << _pin;
    } else {
        _port->OUTSET = 1 << _pin;
    }
}