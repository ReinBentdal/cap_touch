/// This module implementes cap touch using external R with GPIO to create an oscillation.
#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "services/hardware_spec.h"
#include "services/ppi_index.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define _COMP_TH_CALC(mV, Vref_mV) ((64*(mV)/(Vref_mV)) -1)

static void _cap_event(struct k_work* work);
K_WORK_DEFINE(_cap_touch_work, _cap_event);

static void _configure_comparator(int psel);
static void _configure_gpio(int pin);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_ppi(void);

static void _rtc_isr(void);

static int _counter_count_ppi_idx;
static int _counter_clear_ppi_idx;
static int _counter_capture_ppi_idx;

void cap_touch_init(void) {
    LOG_INF("cap_touch_init");
    _counter_count_ppi_idx = ppi_index_get();
    _counter_clear_ppi_idx = ppi_index_get();
    _counter_capture_ppi_idx = ppi_index_get();

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    const int pin = hardware_spec_pin_get(HARDWARE_SPEC_PIN_CAPTOUCH);
    RETURN_ON_ERR_MSG(pin == HARDWARE_SPEC_PIN_UNUSED, "no cap touch pin");

    _configure_comparator(hw_spec->cap_touch_psel);
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
        (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_DRIVE_D0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

    NRF_GPIOTE->CONFIG[0] = 
        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
        (pin << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos) |
        (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);
}

static void _configure_comparator(int psel) {
    __ASSERT_NO_MSG(psel >= 0 && psel <= 7);

    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_VDD << COMP_REFSEL_REFSEL_Pos;
    static const int Vref = 3000;
    static const int offset = 50;

    // set TH for up and down. Change to alter oscillation frequency
    const int th_low = _COMP_TH_CALC(offset, Vref);
    const int th_high = _COMP_TH_CALC(Vref-offset, Vref);
    LOG_INF("th_low: %d, th_high: %d", th_low, th_high);
    NRF_COMP->TH = (th_high << COMP_TH_THUP_Pos) | (th_low << COMP_TH_THDOWN_Pos);

    // set MODE, single ended and low power
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_Low << COMP_MODE_SP_Pos);

    // set pin
    NRF_COMP->PSEL = psel;

    NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Enabled << COMP_ENABLE_ENABLE_Pos;
}

static void _configure_counter(void) {
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;

    NRF_TIMER0->TASKS_CLEAR = 1;
}

static void _configure_rtc(void) {
    NRF_RTC0->PRESCALER = 327; // f = 100Hz
    NRF_RTC0->EVTENSET = RTC_EVTEN_TICK_Enabled << RTC_EVTEN_TICK_Pos;

    // interrupt to log value
    // NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Enabled<< RTC_INTENSET_TICK_Pos; // configure interrupt to log value
    // IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
    // irq_enable(RTC0_IRQn);
}

static void _configure_ppi(void) {

    // connect COMP DOWN evt to GPIO task
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_COMP->EVENTS_DOWN;
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_GPIOTE->TASKS_SET[0];
    NRF_PPI->CHENSET = 1 << 0;

    NRF_PPI->CH[1].EEP = (uint32_t)&NRF_COMP->EVENTS_UP;
    NRF_PPI->CH[1].TEP = (uint32_t)&NRF_GPIOTE->TASKS_CLR[0];
    NRF_PPI->CHENSET = 1 << 1;

    // connect COMP to TIMER0: count
    NRF_PPI->CH[_counter_count_ppi_idx].EEP = (uint32_t)&NRF_COMP->EVENTS_DOWN;
    NRF_PPI->CH[_counter_count_ppi_idx].TEP = (uint32_t)&NRF_TIMER0->TASKS_COUNT;
    NRF_PPI->CHENSET = 1 << _counter_count_ppi_idx;

    // connect COMP to TIMER0: capture timer counter
    // used by interrupt to log the counter value
    NRF_PPI->CH[_counter_capture_ppi_idx].EEP = (uint32_t)&NRF_RTC0->EVENTS_TICK;
    NRF_PPI->CH[_counter_capture_ppi_idx].TEP = (uint32_t)&NRF_TIMER0->TASKS_CAPTURE[1];
    NRF_PPI->CHENSET = 1 << _counter_capture_ppi_idx;

    // connect RTC to TIMER0: reset timer counter
    // TODO: change to FORK
    NRF_PPI->CH[_counter_clear_ppi_idx].EEP = (uint32_t)&NRF_RTC0->EVENTS_TICK;
    NRF_PPI->CH[_counter_clear_ppi_idx].TEP = (uint32_t)&NRF_TIMER0->TASKS_CLEAR;
    NRF_PPI->CHENSET = 1 << _counter_clear_ppi_idx;
}

static void _cap_event(struct k_work* work) {
    LOG_INF("_cap_event");
}

static void _rtc_isr(void) {
    if (NRF_RTC0->EVENTS_TICK) {
        NRF_RTC0->EVENTS_TICK = 0;
        // LOG_INF("Counter read: %d", NRF_TIMER0->CC[1]);
        printk("%d\n", NRF_TIMER0->CC[1]);
    }
}
