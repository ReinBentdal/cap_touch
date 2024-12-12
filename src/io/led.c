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