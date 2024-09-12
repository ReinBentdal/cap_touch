/// This module implementes cap touch using ISOURCE to create an oscillation.
///
/// 

// verify:
// 1. cap oscillating, comp trigger & ISOURCE switch
// 2. counter counting, reset on RTC. Print count val
// 3. Config values for press detection

#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "services/hardware_spec.h"
#include "services/ppi_index.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define Vref_mV (3000) // in reality it varies, but the exact value should not be too important
#define _COMP_TH_CALC(mV) ((64*mV/Vref_mV) -1 )

static void _cap_event(struct k_work* work);
K_WORK_DEFINE(_cap_touch_work, _cap_event);

static void _configure_comparator(int psel);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_ppi(void);

static void _comp_isr(void);
static void _rtc_isr(void);

static int _timer_clear_ppi_idx;
static int _timer_capture_ppi_idx;

void cap_touch_init(void) {
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    uint32_t cap_psel = hardware_spec_pin_get(HARDWARE_SPEC_PIN_CAPTOUCH);
    RETURN_ON_ERR_MSG(cap_psel < 0, "no cap touch pin");

    // _timer_clear_ppi_idx = ppi_index_get();
    // _timer_capture_ppi_idx = ppi_index_get();

    _configure_comparator(cap_psel);

    LOG_INF("cap_touch_init done");
}

void cap_touch_start(void) {
    // NRF_TIMER0->TASKS_START = 1;
    // NRF_TIMER0->TASKS_CLEAR = 1;

    NRF_COMP->TASKS_START = 1;

    // NRF_RTC0->TASKS_START = 1;
}

static void _configure_comparator(int psel) {
    // set voltage reference to VDD
    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_VDD << COMP_REFSEL_REFSEL_Pos;

    // set TH for up and down
    const int th_low = _COMP_TH_CALC(100);
    const int th_high = _COMP_TH_CALC(2900);
    LOG_INF("th_low: %d, th_high: %d", th_low, th_high);
    NRF_COMP->TH = (th_high << COMP_TH_THUP_Pos) | (th_low << COMP_TH_THDOWN_Pos);

    // set MODE, single ended and low power
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_High << COMP_MODE_SP_Pos);

    // enable current source
    NRF_COMP->ISOURCE = COMP_ISOURCE_ISOURCE_Ien10mA << COMP_ISOURCE_ISOURCE_Pos;

    // set pin
    NRF_COMP->PSEL = COMP_PSEL_PSEL_AnalogInput1;

    // set COMP to trigger on DOWN event
    NRF_COMP->INTENSET = COMP_INTEN_DOWN_Enabled << COMP_INTEN_DOWN_Pos;
    
    // enable and start
    NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Enabled << COMP_ENABLE_ENABLE_Pos;

    IRQ_CONNECT(COMP_LPCOMP_IRQn, 3, _comp_isr, 0, 0);
    irq_enable(COMP_LPCOMP_IRQn);
}

static void _configure_counter(void) {
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;

    NRF_TIMER0->TASKS_CLEAR = 1;

    // use compare to trigger a cap touch press event. The RTC should normally reset the timer before this happens
    // NRF_TIMER0->CC[0] = 0xFFFF;

    // NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
}

static void _configure_rtc(void) {
    // use RTC to trigger counter reset on CC using PPI

    // some generic values for now
    NRF_RTC0->PRESCALER = 0;
    NRF_RTC0->CC[0] = 1000;

    NRF_RTC0->EVTENSET = RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos;
    
    // configure interrupt to log value
    NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE0_Enabled << RTC_INTENSET_COMPARE0_Pos;

    IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
}

static void _configure_ppi(void) {
    // its possible to add them all to a group to be able to enable and disable them all at once using task

    // connect COMP to TIMER0: capture timer counter
    // used by interrupt to log the counter value
    NRF_PPI->CH[_timer_capture_ppi_idx].EEP = (uint32_t)&NRF_RTC0->EVENTS_COMPARE[0];
    NRF_PPI->CH[_timer_capture_ppi_idx].TEP = (uint32_t)&NRF_TIMER0->TASKS_CAPTURE[1];
    NRF_PPI->CHENSET = 1 << _timer_capture_ppi_idx;

    // connect RTC to TIMER0: reset timer counter
    NRF_PPI->CH[_timer_clear_ppi_idx].EEP = (uint32_t)&NRF_RTC0->EVENTS_COMPARE[0];
    NRF_PPI->CH[_timer_clear_ppi_idx].TEP = (uint32_t)&NRF_TIMER0->TASKS_CLEAR;
    NRF_PPI->CHENSET = 1 << _timer_clear_ppi_idx;
}

static void _cap_event(struct k_work* work) {
    LOG_INF("_cap_event");
}

static void _comp_isr(void) {
    if (NRF_COMP->EVENTS_DOWN) {
        NRF_COMP->EVENTS_DOWN = 0;
        k_work_submit(&_cap_touch_work);
        // LOG_INF("DOWN");
    }
}

static void _rtc_isr(void);
