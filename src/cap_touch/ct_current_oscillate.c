/// This module implementes cap touch using ISOURCE to create an oscillation.
#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"
#include "nrfx_ppi.h"

#include "services/hardware_spec.h"
#include "utils/ppi_connect.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_DBG);

#define TIMER_SELECT NRF_TIMER2
#define RTC_SELECT NRF_RTC2
#define RTC_IRQn RTC2_IRQn
#define EGU_SELECT NRF_EGU2
#define EGU_IRQn SWI2_EGU2_IRQn

static const uint32_t TOUCH_MIN_PRESS = 75;
static const uint32_t TOUCH_MAX_PRESS = 25;

static const uint8_t RTC_PRESCALAR_DEFAULT = 5;
static const uint8_t RTC_CC0_DEFAULT = 2;
static const uint16_t RTC_CC1_DEFAULT = 800;
static const uint16_t RTC_CC1_FAST = 100;

static uint32_t _ppi_group_autonomous;

static void _configure_comparator(int psel);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_egu(void);
static void _configure_ppi(void);

#ifdef CONFIG_DEBUG
static void _rtc_isr(void);
static void _sample_log(struct k_work *work);
static K_WORK_DEFINE(_sample_log_work, _sample_log);
#endif
static void _touch_detect(void);

static void _event_generate(struct k_work *work);
static K_WORK_DEFINE(_event_generate_work, _event_generate);

static cap_touch_event_t _cb;
static uint32_t _sample;

void cap_touch_init(cap_touch_event_t event_cb) {
    LOG_INF("cap_touch_init");

    __ASSERT_NO_MSG(event_cb != NULL);
    _cb = event_cb;

    LOG_DBG("hardware spec");
    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == HARDWARE_SPEC_PIN_NOT_DEFINED, "no cap touch analog pin");

    LOG_DBG("comp init");
    _configure_comparator(hw_spec->cap_touch_psel);
    LOG_DBG("counter init");
    _configure_counter();
    LOG_DBG("rtc init");
    _configure_rtc();
    LOG_DBG("egu init");
    _configure_egu();
    LOG_DBG("ppi init");
    _configure_ppi();

    LOG_INF("cap_touch_init done");
}

void cap_touch_start(void) {
    LOG_INF("cap_touch_start");

    // make sure connections are made
    NRF_PPI->TASKS_CHG[_ppi_group_autonomous].EN = 1;

    NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Enabled << COMP_ENABLE_ENABLE_Pos;
    NRF_COMP->TASKS_START = 1;

    TIMER_SELECT->TASKS_CLEAR = 1;
    TIMER_SELECT->TASKS_START = 1;

    RTC_SELECT->TASKS_START = 1;
}

void cap_touch_stop(void) {
    LOG_INF("cap_touch_stop");
    RTC_SELECT->TASKS_STOP = 1;

    TIMER_SELECT->TASKS_STOP = 1;

    NRF_COMP->TASKS_STOP = 1;
    NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Disabled << COMP_ENABLE_ENABLE_Pos;
}

static void _configure_comparator(int psel) {
    // set voltage reference to VDD
    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_Int1V2 << COMP_REFSEL_REFSEL_Pos;
    static const int TH_OFFSET_LOW = 2;
    static const int TH_MAX = 63;
    NRF_COMP->TH = ((TH_MAX) << COMP_TH_THUP_Pos) | (TH_OFFSET_LOW << COMP_TH_THDOWN_Pos);

    // set MODE, single ended and low power
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_Low << COMP_MODE_SP_Pos);
    NRF_COMP->ISOURCE = COMP_ISOURCE_ISOURCE_Ien10mA << COMP_ISOURCE_ISOURCE_Pos;

    NRF_COMP->PSEL = psel;
}

static void _configure_counter(void) {
    /* do checks to see if its already in use */
    __ASSERT_NO_MSG(TIMER_SELECT->SHORTS == 0);
    __ASSERT_NO_MSG(TIMER_SELECT->MODE == 0);
    __ASSERT_NO_MSG(TIMER_SELECT->BITMODE == 0);
    __ASSERT(TIMER_SELECT->PRESCALER == 0b0100, "Prescalar already set: %d", TIMER_SELECT->PRESCALER);

    TIMER_SELECT->MODE = TIMER_MODE_MODE_LowPowerCounter << TIMER_MODE_MODE_Pos;
    TIMER_SELECT->BITMODE = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;  // TODO: reduce bitmode when known max count
    TIMER_SELECT->CC[0] = TOUCH_MIN_PRESS;
}

#define RTC_PRESCALAR_HZ(val) (32768/val); BUILD_ASSERT((32768/val) <= 4096, "RTC prescalar must be less than 4096")
static void _configure_rtc(void) {
    /* check if RTC is already in use */
    __ASSERT_NO_MSG(RTC_SELECT->EVTEN == 0);

    RTC_SELECT->PRESCALER = RTC_PRESCALAR_DEFAULT;
    RTC_SELECT->CC[0] = RTC_CC0_DEFAULT;
    RTC_SELECT->CC[1] = RTC_CC1_DEFAULT; 
    RTC_SELECT->EVTENSET = RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos
        | RTC_EVTEN_COMPARE1_Enabled << RTC_EVTEN_COMPARE1_Pos
        ;

#ifdef CONFIG_DEBUG
    // interrupt to log value
    RTC_SELECT->INTENSET = RTC_INTENSET_COMPARE0_Msk
        ; // configure interrupt to log value
    IRQ_CONNECT(RTC_IRQn, 3, _rtc_isr, 0, 0);
    irq_enable(RTC_IRQn);
#endif
}

static void _configure_egu(void) {
    /* do checks to see if its already in use */
    __ASSERT_NO_MSG(EGU_SELECT->INTEN == 0);

    EGU_SELECT->INTENSET = EGU_INTENSET_TRIGGERED0_Msk;
    IRQ_CONNECT(EGU_IRQn, 3, _touch_detect, 0, 0);
    irq_enable(EGU_IRQn);
}

static void _configure_ppi(void) {
    const uint32_t ppi_group_isr = ppi_new_group_find();
    LOG_DBG("ppi_group isr: %d", ppi_group_isr);

    const uint32_t ppi_group_autonomous = ppi_new_group_find();
    // const uint32_t ppi_group_autonomous = 2;
    _ppi_group_autonomous = ppi_group_autonomous;
    LOG_DBG("ppi_group autonomous: %d", ppi_group_autonomous);

    // connect COMP to Count timer
    int ppi_comp_count = ppi_connect((uint32_t)&NRF_COMP->EVENTS_CROSS, (uint32_t)&TIMER_SELECT->TASKS_COUNT);
    LOG_DBG("ppi_connect count: %d", ppi_comp_count);

    // connect Timer CC to group disable
    const unsigned int timer_comp_evt = ppi_connect((uint32_t)&TIMER_SELECT->EVENTS_COMPARE[0], (uint32_t)&NRF_PPI->TASKS_CHG[ppi_group_isr].DIS);
    LOG_DBG("ppi_connect timer comp: %d", timer_comp_evt);

    // PWM OFF
    const unsigned int ppi_pwm_off = ppi_connect((uint32_t)&RTC_SELECT->EVENTS_COMPARE[0], (uint32_t)&NRF_COMP->TASKS_STOP);
    LOG_DBG("ppi_connect pwm off: %d", ppi_pwm_off);

    const unsigned int ppi_pwm_off1 = ppi_connect((uint32_t)&RTC_SELECT->EVENTS_COMPARE[0], (uint32_t)&TIMER_SELECT->TASKS_CAPTURE[1]);
    ppi_fork(ppi_pwm_off1, (uint32_t)&TIMER_SELECT->TASKS_CLEAR);
    LOG_DBG("ppi_connect pwm off 2: %d", ppi_pwm_off1);

    const unsigned int ppi_egu = ppi_connect((uint32_t)&RTC_SELECT->EVENTS_COMPARE[0], (uint32_t)&EGU_SELECT->TASKS_TRIGGER[0]);
    ppi_fork(ppi_egu, (uint32_t)&NRF_PPI->TASKS_CHG[_ppi_group_autonomous].DIS);
    LOG_DBG("ppi_connect egu: %d", ppi_egu);

    // PWM ON
    const unsigned int ppi_pwm_on = ppi_connect((uint32_t)&RTC_SELECT->EVENTS_COMPARE[1], (uint32_t)&NRF_COMP->TASKS_START);
    ppi_fork(ppi_pwm_on, (uint32_t)&RTC_SELECT->TASKS_CLEAR);
    LOG_DBG("ppi_connect pwm on: %d", ppi_pwm_on);
    const unsigned int ppi_pwm_on1 = ppi_connect((uint32_t)&RTC_SELECT->EVENTS_COMPARE[1], (uint32_t)&NRF_PPI->TASKS_CHG[ppi_group_isr].EN);
    LOG_DBG("ppi_connect pwm on2: %d", ppi_pwm_on1);

    NRF_PPI->CHG[ppi_group_isr] = 1 << ppi_egu;
    NRF_PPI->CHG[_ppi_group_autonomous] = 1 << timer_comp_evt;



    // disabling TIMER makes absolutely no difference in power consumption
    // ppi_fork(ppi_pwm_off, (uint32_t)&TIMER_SELECT->TASKS_STOP);
    // ppi_fork(ppi_pwm_on1, (uint32_t)&TIMER_SELECT->TASKS_START);
    ARG_UNUSED(ppi_pwm_off);
    ARG_UNUSED(ppi_pwm_on1);
}

#ifdef CONFIG_DEBUG
static void _rtc_isr(void) {
    if (RTC_SELECT->EVENTS_COMPARE[0]) {
        RTC_SELECT->EVENTS_COMPARE[0] = 0;
        k_work_submit(&_sample_log_work);
    }
}

static void _sample_log(struct k_work *work) {
    LOG_DBG("Sample: %d", TIMER_SELECT->CC[1]);
}
#endif

static bool _did_wake = true;
static void _touch_detect(void) {
    if (EGU_SELECT->EVENTS_TRIGGERED[0]) {
        EGU_SELECT->EVENTS_TRIGGERED[0] = 0;
        _sample = TIMER_SELECT->CC[1];

        if (_did_wake) {
            LOG_DBG("Increasing frequency");
            _did_wake = false;
            // assume quick enough
            RTC_SELECT->CC[1] = RTC_CC1_FAST;
        }


        int ret = k_work_submit(&_event_generate_work);
        if (ret != 1 && ret != 2) {
            LOG_ERR("Failed to submit work: %d", ret);
        }
    }
}

static void _event_generate(struct k_work *work) {
    LOG_DBG("Touch detected: %d", _sample);
    static const uint8_t scale_factor = 100;
    static uint8_t prev = 0;

    /* go back to autonomous operation if sample is outside of range */
    static const uint32_t TOUCH_HYST_PERCENT = 5;
    if (_sample > TOUCH_MIN_PRESS * (100 + TOUCH_HYST_PERCENT) / 100) {
        LOG_DBG("Going back to autonomous operation");
        NRF_PPI->TASKS_CHG[_ppi_group_autonomous].EN = 1;
        _did_wake = true;

        // reset RTC
        LOG_DBG("Resetting RTC");
        RTC_SELECT->CC[1] = RTC_CC1_DEFAULT; // From testing, COUNTER is well within range from it being greater than CC1
        LOG_DBG("RTC: %d, CC: %d", RTC_SELECT->COUNTER, RTC_SELECT->CC[1]);

        prev = 0;
        _cb(0);

        return;
    }

    /* generate a value ranging from 0 to 127 */
    const int32_t value = (TOUCH_MIN_PRESS - _sample) * 127 / (TOUCH_MIN_PRESS - TOUCH_MAX_PRESS);
    const uint32_t value_clamp = CLAMP(value, 0, 127);

    /* scale value between prev and current */
    const uint32_t scaled = (prev*scale_factor + value_clamp*(UINT8_MAX - scale_factor) + 128) >> 8; // 8: scale factor, 7: raw value
    if (prev == scaled) return;
    prev = scaled;
    _cb(scaled);
}