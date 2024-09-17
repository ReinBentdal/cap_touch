/// This module implementes cap touch using ISOURCE to create an oscillation.
#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "services/hardware_spec.h"
#include "utils/ppi_connect.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define _COMP_TH_CALC(mV, Vref_mV) ((64*(mV)/(Vref_mV)) -1)

static void _cap_event(struct k_work* work);
K_WORK_DEFINE(_cap_touch_work, _cap_event);

static void _configure_comparator(int psel);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_ppi(void);

static void _rtc_isr(void);

void cap_touch_init(void) {
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    _configure_comparator(hw_spec->cap_touch_psel);
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

static void _configure_comparator(int psel) {
    // set voltage reference to VDD
    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_Int1V2 << COMP_REFSEL_REFSEL_Pos;
    static const int Vref = 1200;
    static const int offset = 50;

    // set TH for up and down
    const int th_low = _COMP_TH_CALC(offset, Vref);
    const int th_high = _COMP_TH_CALC(Vref-offset, Vref);
    LOG_INF("th_low: %d, th_high: %d", th_low, th_high);
    NRF_COMP->TH = (th_high << COMP_TH_THUP_Pos) | (th_low << COMP_TH_THDOWN_Pos);

    // set MODE, single ended and low power
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_Low << COMP_MODE_SP_Pos);

    // enable current source
    NRF_COMP->ISOURCE = COMP_ISOURCE_ISOURCE_Ien2mA5 << COMP_ISOURCE_ISOURCE_Pos;

    // set pin
    NRF_COMP->PSEL = psel;
    
    // enable and start
    NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Enabled << COMP_ENABLE_ENABLE_Pos;
}

static void _configure_counter(void) {
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;

    NRF_TIMER0->TASKS_CLEAR = 1;
}

#define RTC_PRESCALAR_HZ(val) (32768/val); BUILD_ASSERT((32768/val) <= 4096, "RTC prescalar must be less than 4096")
static void _configure_rtc(void) {
    NRF_RTC0->PRESCALER = RTC_PRESCALAR_HZ(10);
    NRF_RTC0->EVTENSET = RTC_EVTEN_TICK_Enabled << RTC_EVTEN_TICK_Pos;

    // interrupt to log value
    NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Enabled<< RTC_INTENSET_TICK_Pos; // configure interrupt to log value
    IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
    irq_enable(RTC0_IRQn);
}

static void _configure_ppi(void) {
    // connect COMP to TIMER0: count
    (void)ppi_connect((uint32_t)&NRF_COMP->EVENTS_CROSS, (uint32_t)&NRF_TIMER0->TASKS_COUNT);

    // connect COMP to TIMER0: capture timer counter. used by interrupt to log the counter value
    const unsigned int ppi_idx = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_TICK, (uint32_t)&NRF_TIMER0->TASKS_CAPTURE[1]);
    ppi_fork(ppi_idx, (uint32_t)&NRF_TIMER0->TASKS_CLEAR);
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
