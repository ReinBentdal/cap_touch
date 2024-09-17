/// This module implementes cap touch using ISOURCE to create an oscillation.
#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "services/hardware_spec.h"
#include "utils/ppi_connect.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define PERIOD_TIMER NRF_TIMER0
#define DETECT_TIMER NRF_TIMER1

static void _configure_comparator(int psel);
static void _configure_counter(void);
static void _configure_rtc(void);

static void _rtc_isr(void);
static void _touch_detect(void);

void cap_touch_init(void) {
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    _configure_comparator(hw_spec->cap_touch_psel);
    _configure_counter();
    _configure_rtc();

    LOG_INF("cap_touch_init done");
}

void cap_touch_start(void) {
    LOG_INF("cap_touch_start");

    PERIOD_TIMER->TASKS_CLEAR = 1;
    PERIOD_TIMER->TASKS_START = 1;

    DETECT_TIMER->TASKS_CLEAR = 1;
    DETECT_TIMER->TASKS_START = 1;

    NRF_COMP->TASKS_START = 1;
    NRF_RTC0->TASKS_START = 1;
}

#define _COMP_TH_CALC(mV, Vref_mV) ((64*(mV)/(Vref_mV)) -1)
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

    // connect COMP to TIMER0: period count
    (void)ppi_connect((uint32_t)&NRF_COMP->EVENTS_CROSS, (uint32_t)&PERIOD_TIMER->TASKS_COUNT);
}

static void _configure_counter(void) {
    // TODO: reduce bitmode when known max count
    PERIOD_TIMER->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    PERIOD_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;
    PERIOD_TIMER->CC[0] = 30; // rough value to detect touch

    ppi_connect((uint32_t)&PERIOD_TIMER->EVENTS_COMPARE[0], (uint32_t)&DETECT_TIMER->TASKS_CLEAR);

    DETECT_TIMER->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    DETECT_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;
    DETECT_TIMER->CC[0] = 2; // concecutively detect touch
    DETECT_TIMER->INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;

    ppi_connect((uint32_t)&DETECT_TIMER->EVENTS_COMPARE[0], (uint32_t)&DETECT_TIMER->TASKS_CLEAR);

    // IRQ_CONNECT(TIMER1_IRQn, 3, _touch_detect, 0, 0);
    // irq_enable(TIMER1_IRQn);
}

#define RTC_PRESCALAR_HZ(val) (32768/val); BUILD_ASSERT((32768/val) <= 4096, "RTC prescalar must be less than 4096")
static void _configure_rtc(void) {
    static const uint32_t PWM_ON_TICKS = 2;
    static const uint32_t DUTY = 1;
    static const uint32_t FPS = 10;

    static const uint32_t PWM_PERIOD_TICKS = PWM_ON_TICKS*(100/DUTY);
    static const uint32_t TICK_FREQUENCY = PWM_PERIOD_TICKS*FPS;
    NRF_RTC0->PRESCALER = RTC_PRESCALAR_HZ(TICK_FREQUENCY);
    NRF_RTC0->EVTENSET = RTC_EVTEN_TICK_Enabled << RTC_EVTEN_TICK_Pos;

    // trigger TIMER clear and capture from tick
    const unsigned int ppi_tick = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_TICK, (uint32_t)&PERIOD_TIMER->TASKS_CAPTURE[1]);
    ppi_fork(ppi_tick, (uint32_t)&PERIOD_TIMER->TASKS_CLEAR);
    // increment detect timer
    const unsigned int ppi_tick_detect = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_TICK, (uint32_t)&DETECT_TIMER->TASKS_COUNT);

    NRF_PPI->CHG[0] = 1 << ppi_tick
        | 1 << ppi_tick_detect
        ;

    // PWM on and off system
    NRF_RTC0->CC[0] = PWM_ON_TICKS; // off
    NRF_RTC0->CC[1] = PWM_PERIOD_TICKS; // on

    NRF_RTC0->EVTENSET = RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos
        | RTC_EVTEN_COMPARE1_Enabled << RTC_EVTEN_COMPARE1_Pos
        ;

    const unsigned int ppi_pwm_off = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[0], (uint32_t)&NRF_COMP->TASKS_STOP);
    ppi_fork(ppi_pwm_off, (uint32_t)&NRF_PPI->TASKS_CHG[0].DIS);

    const unsigned int ppi_pwm_on = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[1], (uint32_t)&NRF_COMP->TASKS_START);
    ppi_fork(ppi_pwm_on, (uint32_t)&NRF_RTC0->TASKS_CLEAR);
    (void)ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[1], (uint32_t)&NRF_PPI->TASKS_CHG[0].EN);

    // interrupt to log value
    // NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Enabled<< RTC_INTENSET_TICK_Pos 
    //     | RTC_INTENSET_COMPARE0_Enabled << RTC_INTENSET_COMPARE0_Pos
    //     | RTC_INTENSET_COMPARE1_Enabled << RTC_INTENSET_COMPARE1_Pos
    //     ; // configure interrupt to log value
    // IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
    // irq_enable(RTC0_IRQn);
}

static void _rtc_isr(void) {
    if (NRF_RTC0->EVENTS_TICK) {
        NRF_RTC0->EVENTS_TICK = 0;
        if (NRF_RTC0->COUNTER <= 2) {
            printk("%d\n", PERIOD_TIMER->CC[1]);
        }
    }
    if (NRF_RTC0->EVENTS_COMPARE[0]) {
        NRF_RTC0->EVENTS_COMPARE[0] = 0;
       printk("PWM off\n");
    }
    if (NRF_RTC0->EVENTS_COMPARE[1]) {
        NRF_RTC0->EVENTS_COMPARE[1] = 0;
        printk("PWM on\n");
    }
}

static void _touch_detect(void) {
    if (DETECT_TIMER->EVENTS_COMPARE[0]) {
        DETECT_TIMER->EVENTS_COMPARE[0] = 0;
        printk("touch detected: %d\n", PERIOD_TIMER->CC[1]);
    }
}