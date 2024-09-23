/// This module implementes cap touch using ISOURCE to create an oscillation.
#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "services/hardware_spec.h"
#include "utils/ppi_connect.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define COUNT_TIMER NRF_TIMER0

static void _configure_comparator(int psel);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_egu(void);
static void _configure_ppi(void);

static void _rtc_isr(void);
static void _touch_detect(void);

void cap_touch_init(void) {
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    _configure_comparator(hw_spec->cap_touch_psel);
    _configure_counter();
    _configure_rtc();
    _configure_egu();
    _configure_ppi();

    LOG_INF("cap_touch_init done");
}

void cap_touch_start(void) {
    LOG_INF("cap_touch_start");

    COUNT_TIMER->TASKS_CLEAR = 1;
    COUNT_TIMER->TASKS_START = 1;

    NRF_COMP->TASKS_START = 1;
    NRF_RTC0->TASKS_START = 1;
}

#define _COMP_TH_CALC(mV, Vref_mV) ((64*(mV)/(Vref_mV)) -1)
static void _configure_comparator(int psel) {
    // set voltage reference to VDD
    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_Int1V2 << COMP_REFSEL_REFSEL_Pos;
    static const int Vref = 1200;
    static const int offset = 50;
    NRF_COMP->TH = (_COMP_TH_CALC(Vref-offset, Vref) << COMP_TH_THUP_Pos) | (_COMP_TH_CALC(offset, Vref) << COMP_TH_THDOWN_Pos);

    // set MODE, single ended and low power
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_Low << COMP_MODE_SP_Pos);
    NRF_COMP->ISOURCE = COMP_ISOURCE_ISOURCE_Ien10mA << COMP_ISOURCE_ISOURCE_Pos;

    NRF_COMP->PSEL = psel;
    NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Enabled << COMP_ENABLE_ENABLE_Pos;
}

static void _configure_counter(void) {
    COUNT_TIMER->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    COUNT_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;  // TODO: reduce bitmode when known max count
    COUNT_TIMER->CC[0] = 10; // rough value to detect touch
}

#define RTC_PRESCALAR_HZ(val) (32768/val); BUILD_ASSERT((32768/val) <= 4096, "RTC prescalar must be less than 4096")
static void _configure_rtc(void) {
    NRF_RTC0->PRESCALER = 5; // use prescalar to set operating frequency
    NRF_RTC0->CC[0] = 1; // PWM off, results in on time of 5/32768 = 150µs.
    NRF_RTC0->CC[1] = 800; // PWM on, total period of 800*5/32768 = 122ms
    NRF_RTC0->EVTENSET = RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos
        | RTC_EVTEN_COMPARE1_Enabled << RTC_EVTEN_COMPARE1_Pos
        ;

#ifdef CONFIG_DEBUG
    // interrupt to log value
    NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Enabled<< RTC_INTENSET_TICK_Pos 
        | RTC_INTENSET_COMPARE0_Enabled << RTC_INTENSET_COMPARE0_Pos
        ; // configure interrupt to log value
    IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
    irq_enable(RTC0_IRQn);
#endif
}

static void _configure_egu(void) {
    NRF_EGU0->INTENSET = EGU_INTENSET_TRIGGERED0_Enabled << EGU_INTENSET_TRIGGERED0_Pos;
    IRQ_CONNECT(SWI0_EGU0_IRQn, 3, _touch_detect, 0, 0);
    irq_enable(SWI0_EGU0_IRQn);
}

static void _configure_ppi(void) {
    static const uint32_t CHG_IDX = 0;
    // connect COMP to Count timer
    (void)ppi_connect((uint32_t)&NRF_COMP->EVENTS_CROSS, (uint32_t)&COUNT_TIMER->TASKS_COUNT);

    // connect Timer CC to group disable
    (void)ppi_connect((uint32_t)&COUNT_TIMER->EVENTS_COMPARE[0], (uint32_t)&NRF_PPI->TASKS_CHG[CHG_IDX].DIS);

    // PWM OFF
    const unsigned int ppi_owm_off = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[0], (uint32_t)&NRF_COMP->TASKS_STOP);

    const unsigned int ppi_pwm_off1 = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[0], (uint32_t)&COUNT_TIMER->TASKS_CAPTURE[1]);
    ppi_fork(ppi_pwm_off1, (uint32_t)&COUNT_TIMER->TASKS_CLEAR);

    const unsigned int ppi_pwm_egu = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[0], (uint32_t)&NRF_EGU0->TASKS_TRIGGER[0]);
    NRF_PPI->CHG[CHG_IDX] = 1 << ppi_pwm_egu;

    // PWM ON
    const unsigned int ppi_pwm_on = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[1], (uint32_t)&NRF_COMP->TASKS_START);
    ppi_fork(ppi_pwm_on, (uint32_t)&NRF_RTC0->TASKS_CLEAR);
    const unsigned int ppi_pwm_on1 = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[1], (uint32_t)&NRF_PPI->TASKS_CHG[CHG_IDX].EN);
    
    // disabling TIMER makes absolutely no difference in power consumption
    // ppi_fork(ppi_owm_off, (uint32_t)&COUNT_TIMER->TASKS_STOP);
    // ppi_fork(ppi_pwm_on1, (uint32_t)&COUNT_TIMER->TASKS_START);
    ARG_UNUSED(ppi_owm_off);
    ARG_UNUSED(ppi_pwm_on1);
}

static void _rtc_isr(void) {
    if (NRF_RTC0->EVENTS_COMPARE[0]) {
        NRF_RTC0->EVENTS_COMPARE[0] = 0;
       printk("PWM off, capture %d\n", COUNT_TIMER->CC[1]);
    }
}

static void _touch_detect(void) {
    if (NRF_EGU0->EVENTS_TRIGGERED[0]) {
        NRF_EGU0->EVENTS_TRIGGERED[0] = 0;
        printk("Touch detected: %d\n", COUNT_TIMER->CC[1]);
    }
}