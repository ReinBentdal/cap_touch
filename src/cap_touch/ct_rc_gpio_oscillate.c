/// ATTENTION: NOT WORKING

/*
 * File: ct_rc_gpio_oscillate.c
 * Author: Rein Gundersen Bentdal
 * Created: 19.Des 2024
 * Description: This module implementes cap touch using external R with GPIO to create an oscillation.
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

#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "utils/ppi_connect.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define GPIOTE_TOGGLE_IDX 0
#define GPIOTE_CAPTURE_IDX 1

static void _cap_event(struct k_work* work);
K_WORK_DEFINE(_cap_touch_work, _cap_event);

static void _configure_gpio(int pin);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_ppi(void);

static void _rtc_isr(void);
static void _gpio_isr(void);

void cap_touch_init(cap_touch_event_t event_cb, uint32_t comp_psel) {
    ARG_UNUSED(event_cb);
    ARG_UNUSED(comp_psel);
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");
    const int pin = hardware_spec_pin_get(HARDWARE_SPEC_PIN_CAPTOUCH);
    RETURN_ON_ERR_MSG(pin == HARDWARE_SPEC_PIN_UNUSED, "no cap touch pin");
    LOG_INF("cap_touch_init pin: %d", pin);

    _configure_gpio(pin);
    _configure_counter();
    _configure_rtc();
    _configure_ppi();

    LOG_INF("cap_touch_init done");
}

void cap_touch_start(void) {
    LOG_INF("cap_touch_start");

    NRF_TIMER0->TASKS_CLEAR = 1;
    NRF_TIMER0->TASKS_START = 1;

    NRF_COMP->TASKS_START = 1;
    NRF_RTC0->TASKS_START = 1;
}

static void _configure_gpio(int pin) {
    __ASSERT_NO_MSG(pin >= 0);

    NRF_GPIO->PIN_CNF[pin] = 
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
        | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
        | (GPIO_PIN_CNF_DRIVE_D0S1 << GPIO_PIN_CNF_DRIVE_Pos)
        | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
        ;

    // configure one GPIOTE crossing event
    NRF_GPIOTE->CONFIG[GPIOTE_CAPTURE_IDX] = 
        (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
        | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
        | ((pin+1) << GPIOTE_CONFIG_PSEL_Pos)
        | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos)
        ;

    // config one GPIOTE to toggle the pin
    NRF_GPIOTE->CONFIG[GPIOTE_TOGGLE_IDX] = 
        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
        | (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos)
        | ((pin) << GPIOTE_CONFIG_PSEL_Pos)
        | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos)
        ;

    // NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_IN0_Enabled << GPIOTE_IDX;
    // IRQ_CONNECT(GPIOTE_IRQn, 3, _gpio_isr, 0, 0);
    // irq_enable(GPIOTE_IRQn);
}

static void _configure_counter(void) {
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;

    NRF_TIMER0->TASKS_CLEAR = 1;
}

#define RTC_PRESCALAR_HZ(val) (32768/val); BUILD_ASSERT((32768/val) <= 4096, "RTC prescalar must be less than 4096")
static void _configure_rtc(void) {
    NRF_RTC0->PRESCALER = RTC_PRESCALAR_HZ(10); // f = 10Hz
    NRF_RTC0->EVTENSET = RTC_EVTEN_TICK_Enabled << RTC_EVTEN_TICK_Pos;

    // interrupt to log value
    NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Enabled<< RTC_INTENSET_TICK_Pos; // configure interrupt to log value
    IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
    irq_enable(RTC0_IRQn);
}

static void _configure_ppi(void) {
    // GPIOTE crossing to toggle
    // const unsigned int ppi_gpiote = ppi_connect((uint32_t)&NRF_GPIOTE->EVENTS_IN[GPIOTE_CAPTURE_IDX], (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_TOGGLE_IDX]); 
    // ppi_fork(ppi_gpiote, (uint32_t)&NRF_TIMER0->TASKS_COUNT);

    // RTC tick resetting counter
    const unsigned int ppi_tick_idx = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_TICK, (uint32_t)&NRF_TIMER0->TASKS_CAPTURE[1]);
    ppi_fork(ppi_tick_idx, (uint32_t)&NRF_TIMER0->TASKS_CLEAR);

}

static void _cap_event(struct k_work* work) {
    LOG_INF("_cap_event");
}

static void _rtc_isr(void) {
    if (NRF_RTC0->EVENTS_TICK) {
        NRF_RTC0->EVENTS_TICK = 0;
        printk("%d\n", NRF_TIMER0->CC[1]);
    }
}

static void _gpio_isr(void) {
    if (NRF_GPIOTE->EVENTS_IN[GPIOTE_TOGGLE_IDX]) {
        NRF_GPIOTE->EVENTS_IN[GPIOTE_TOGGLE_IDX] = 0;
        LOG_INF("GPIO event");
    }
}