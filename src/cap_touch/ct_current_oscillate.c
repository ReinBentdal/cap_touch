/** This module implementes cap touch using COMP ISOURCE to create an oscillation. It only requires one pin.
 * The relative capacitance is measured by counting the number of oscillations (N) in a given time period, using a timer in counter mode. The count value is inversely proportional to the capacitance.
 * 
 * When running, it operates in two modes:
 * - _STATE_AUTONOMOUS_LOW_FREQUENCY: running at low frequency and low resolution, independent from the CPU. CPU interrupt is only triggered if a touch is detected
 * - _STATE_HIGH_FREQUENCY: running at high frequency and high resolution. An interrupt is triggered for each new sample, which is then processed by the CPU
 * 
 * The system auto-calibrates by keeping track of the highest period count (N) and using this as a scaled reference. The assumption is that the highest period count is when the capacitance is at its lowest, 
 * i.e. when there is no added capacitance from external factors. The calibration is done periodically, and filtered using a median filter. Furthermore, the system generates a calibration point from both _STATE_AUTONOMOUS_LOW_FREQUENCY
 * and _STATE_HIGH_FREQUENCY, becasue the two modes have different resolution and yields different period counts. Calibration in both modes is necessary to prevent deadlock (mallformed calibration point resulting in the system at 
 * idle being in _STATE_HIGH_FREQUENCY).
*/

#include "cap_touch.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "utils/ppi_connect.h"
#include "utils/macros_common.h"
#include "utils/sorted_index_get.h"

#include "bluetooth/bt_log.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_DBG);

struct _counter_region {
    uint32_t nominal;
    uint32_t activate;
    uint32_t saturate;
};

enum _state {
    _STATE_UNINITIALIZED = 0,
    _STATE_NOT_SUPPORTED,
    _STATE_OFF,
    _STATE_AUTONOMOUS_LOW_FREQUENCY,
    _STATE_HIGH_FREQUENCY,
};
#define _STATE_TRANSITION(from, to) ((from) << 8 | (to))

#define _FIXED8_PERCENT(percent) ((percent) * (255) / 100)

/* Resource selection */
#define COUNTER_SELECT NRF_TIMER2
#define RTC_SELECT NRF_RTC2
#define RTC_IRQn RTC2_IRQn
#define EGU_SELECT NRF_EGU2
#define EGU_IRQn SWI2_EGU2_IRQn

#define RTC_CC_SAMPLE_START_IDX 0
#define RTC_CC_SAMPLE_END_IDX 1
#define RTC_CC_RESET_IDX 2

#define RTC_CC_SAMPLE_START_VALUE 1

#define EGU_ACTIVATE_IDX 0

#define COUNTER_CC_ACTIVE_TRIGGER 0
#define COUNTER_CC_SAMPLE_CAPTURE 1
#define COUNTER_CC_CALIBRATION_CAPTURE_LF 2
#define COUNTER_CC_CALIBRATION_CAPTURE_HF 3

/* operation parameters of _STATE_AUTONOMOUS_LOW_FREQUENCY and _STATE_HIGH_FREQUENCY state */
#define RTC_TICKS_SAMPLE 4                          // current 10uA => 6 ticks, current 2u5 => 14 ticks
#define RTC_TICKS_SAMPLE_HF 500                      // current 10uA => 40 ticks, current 2u5 => 100 ticks
#define RTC_TICKS_RESET_LOW_FREQUENCY 4000
#define RTC_TICKS_RESET_HIGH_FREQUENCY 4000

static enum _state _state = _STATE_UNINITIALIZED;
static struct _counter_region _counter_region;
static uint32_t _ppi_isr_always_activate;
static uint32_t _calibration_period;
static uint32_t _ppi_calibration_lf_compare;
static uint32_t _ppi_calibration_hf_compare;

/* buffer samples from ISR to work handler */
#define _MSGQ_SIZE 4
K_MSGQ_DEFINE(_samples_msgq, sizeof(uint16_t), _MSGQ_SIZE, sizeof(uint16_t));

static void _set_state(enum _state new_state);

static void _configure_comparator(void);
static void _configure_counter(void);
static void _configure_rtc(void);
static void _configure_egu(void);
static void _configure_ppi(void);

#define _CALIBRATION_START_DELAY_MS 10
#define _CALIBRATION_SAMPLE_CAPTURE_PERIOD_MAX_SEC (2*60)
#define _CALIBRATION_SAMPLE_CAPTURE_PERIOD_INIT_SEC (1)
static void _calibration_start(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(_calibration_start_work, _calibration_start);
static void _calibration_reset(void);
static void _calibration_capture(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(_calibration_capture_work, _calibration_capture);

static uint16_t _calibration_buf[5];
static uint8_t _calibration_buf_idx = 0;
    
static void _egu_irq(void);

static void _counter_region_set(uint32_t calibration_point);

static void _sample_process(struct k_work *work);
static K_WORK_DEFINE(_sample_process_work, _sample_process);

static cap_touch_event_t _cb;
void cap_touch_init(cap_touch_event_t event_cb, uint32_t psel_comp, uint32_t psel_pin) {
    __ASSERT_NO_MSG(_state == _STATE_UNINITIALIZED);
    __ASSERT_NO_MSG(event_cb != NULL);
    ARG_UNUSED(psel_pin);
    LOG_INF("cap_touch_init");

    if (psel_comp == -1) {
        LOG_WRN("the board does not have cap touch");
        _set_state(_STATE_NOT_SUPPORTED);
        return;
    }

    _cb = event_cb;
    NRF_COMP->PSEL = psel_comp;

    _set_state(_STATE_OFF);
}

void cap_touch_start(void) {
    LOG_INF("cap_touch_start");
    _set_state(_STATE_AUTONOMOUS_LOW_FREQUENCY);
}

void cap_touch_stop(void) {
    LOG_INF("cap_touch_stop");
    _set_state(_STATE_OFF);
}

/* state machine is implemented such that it's valid to call it at any time, but requires it to be called from only a single thread */
static void _set_state(enum _state new_state) {
    if (_state == new_state) {
        LOG_WRN("Already in state: %d", new_state);
        return;
    }

    switch (_STATE_TRANSITION(_state, new_state)) {
        case _STATE_TRANSITION(_STATE_UNINITIALIZED, _STATE_NOT_SUPPORTED):
            LOG_WRN("STATE_NOT_SUPPORTED");
            break;
        case _STATE_TRANSITION(_STATE_UNINITIALIZED, _STATE_OFF):
            LOG_INF("STATE_OFF, initialising");
            _configure_comparator();
            _configure_counter();
            _configure_rtc();
            _configure_egu();
            _configure_ppi();
            break;

        case _STATE_TRANSITION(_STATE_HIGH_FREQUENCY, _STATE_OFF):
        case _STATE_TRANSITION(_STATE_AUTONOMOUS_LOW_FREQUENCY, _STATE_OFF):
            LOG_INF("STATE_OFF");
            k_work_cancel_delayable(&_calibration_capture_work);
            RTC_SELECT->TASKS_STOP = 1;
            COUNTER_SELECT->TASKS_STOP = 1;
            RTC_SELECT->TASKS_CLEAR = 1;
            COUNTER_SELECT->TASKS_CLEAR = 1;
            NRF_COMP->TASKS_STOP = 1;
            NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Disabled << COMP_ENABLE_ENABLE_Pos;
            break;
        
        case _STATE_TRANSITION(_STATE_OFF, _STATE_AUTONOMOUS_LOW_FREQUENCY):
            NRF_COMP->ENABLE = COMP_ENABLE_ENABLE_Enabled << COMP_ENABLE_ENABLE_Pos;
            NRF_COMP->TASKS_START = 1;
            COUNTER_SELECT->TASKS_START = 1;
            RTC_SELECT->TASKS_START = 1;
            _counter_region_set(0); // initial trigger point
            k_work_schedule(&_calibration_start_work, K_MSEC(_CALIBRATION_START_DELAY_MS)); // wait until system is stable
        case _STATE_TRANSITION(_STATE_HIGH_FREQUENCY, _STATE_AUTONOMOUS_LOW_FREQUENCY):
            LOG_INF("STATE_LOW_FREQUENCY");

            // operation parameters
            RTC_SELECT->CC[RTC_CC_SAMPLE_END_IDX] = RTC_TICKS_SAMPLE + RTC_CC_SAMPLE_START_VALUE;
            RTC_SELECT->CC[RTC_CC_RESET_IDX] = RTC_TICKS_RESET_LOW_FREQUENCY;

            // activate autonompus mode and calibration to LF register
            NRF_PPI->CHENSET = 1 << _ppi_isr_always_activate;
            NRF_PPI->CHENSET = 1 << _ppi_calibration_lf_compare;
            NRF_PPI->CHENCLR = 1 << _ppi_calibration_hf_compare;

            // restart
            RTC_SELECT->TASKS_CLEAR = 1;
            break;

        case _STATE_TRANSITION(_STATE_AUTONOMOUS_LOW_FREQUENCY, _STATE_HIGH_FREQUENCY):
            LOG_INF("STATE_HIGH_FREQUENCY");
            // deactivate autonomous mode and calibration to HF register
            NRF_PPI->CHENCLR = 1 << _ppi_isr_always_activate;
            NRF_PPI->CHENSET = 1 << _ppi_calibration_hf_compare;
            NRF_PPI->CHENCLR = 1 << _ppi_calibration_lf_compare;

            // operation parameters
            RTC_SELECT->CC[RTC_CC_SAMPLE_END_IDX] = RTC_TICKS_SAMPLE_HF;
            RTC_SELECT->CC[RTC_CC_RESET_IDX] = RTC_TICKS_RESET_HIGH_FREQUENCY;

            // restart
            RTC_SELECT->TASKS_CLEAR = 1;
            break;
        
        // not valid transitions (not including unititialized & not supported)
        default:
            LOG_ERR("Invalid state transition: from %d, to %d", _state, new_state);
            return; // <-- not setting new state
    }
    _state = new_state;
}

static void _configure_comparator() {
    #define COMP_TH_MAX 63
    #define COMP_TH_OFFSET_LOW 5
    #define COMP_TH_OFFSET_HIGH 30
    BUILD_ASSERT(COMP_TH_OFFSET_HIGH <= COMP_TH_MAX && COMP_TH_OFFSET_LOW <= COMP_TH_MAX && COMP_TH_OFFSET_HIGH > COMP_TH_OFFSET_LOW, "COMP offsets invalid");
    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_VDD << COMP_REFSEL_REFSEL_Pos;
    NRF_COMP->TH = ((COMP_TH_OFFSET_HIGH) << COMP_TH_THUP_Pos) | (COMP_TH_OFFSET_LOW << COMP_TH_THDOWN_Pos);
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_Low << COMP_MODE_SP_Pos);
    NRF_COMP->ISOURCE = COMP_ISOURCE_ISOURCE_Ien2mA5 << COMP_ISOURCE_ISOURCE_Pos;
}

static void _configure_counter(void) {
    COUNTER_SELECT->MODE = TIMER_MODE_MODE_LowPowerCounter << TIMER_MODE_MODE_Pos;
    COUNTER_SELECT->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
}

static void _configure_rtc(void) {
    RTC_SELECT->EVTENSET = RTC_EVTEN_COMPARE0_Msk | RTC_EVTEN_COMPARE1_Msk | RTC_EVTEN_COMPARE2_Msk;
    RTC_SELECT->CC[RTC_CC_SAMPLE_START_IDX] = RTC_CC_SAMPLE_START_VALUE;
}

static void _configure_egu(void) {
    EGU_SELECT->INTENSET = EGU_INTENSET_TRIGGERED0_Msk;
    IRQ_CONNECT(EGU_IRQn, 3, _egu_irq, 0, 0);
    irq_enable(EGU_IRQn);
}

static void _configure_ppi(void) {
    const uint32_t ppi_group_sample_activate = ppi_new_group_find();            // 1 (reference to ppi connections diagram)
    const uint32_t ppi_group_calibration_capture_lf = ppi_new_group_find();     // 2
    const uint32_t ppi_group_calibration_capture_hf = ppi_new_group_find();     // 3

    // connect COMP to Count timer
    (void)ppi_connect(&NRF_COMP->EVENTS_CROSS, &COUNTER_SELECT->TASKS_COUNT);

    // Timer output CCs. IRQ intercept and calibration compare
    _ppi_isr_always_activate = ppi_connect(&COUNTER_SELECT->EVENTS_COMPARE[COUNTER_CC_ACTIVE_TRIGGER], &NRF_PPI->TASKS_CHG[ppi_group_sample_activate].DIS);
    _ppi_calibration_lf_compare = ppi_connect(&COUNTER_SELECT->EVENTS_COMPARE[COUNTER_CC_CALIBRATION_CAPTURE_LF], &NRF_PPI->TASKS_CHG[ppi_group_calibration_capture_lf].EN);
    _ppi_calibration_hf_compare = ppi_connect(&COUNTER_SELECT->EVENTS_COMPARE[COUNTER_CC_CALIBRATION_CAPTURE_HF], &NRF_PPI->TASKS_CHG[ppi_group_calibration_capture_hf].EN);

    // RTC sampling start
    (void)ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_START_IDX], &NRF_COMP->TASKS_START);
    
    const uint32_t ppi_pwm_on1 = ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_START_IDX], &NRF_PPI->TASKS_CHG[ppi_group_sample_activate].EN);
    ppi_fork(ppi_pwm_on1, &COUNTER_SELECT->TASKS_CLEAR);
    
    const uint32_t ppi_pwm_on_grp = ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_START_IDX], &NRF_PPI->TASKS_CHG[ppi_group_calibration_capture_lf].DIS);
    ppi_fork(ppi_pwm_on_grp, &NRF_PPI->TASKS_CHG[ppi_group_calibration_capture_hf].DIS);

    // RTC sampling end
    const uint32_t ppi_pwm_off = ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_END_IDX], &NRF_COMP->TASKS_STOP);
    ARG_UNUSED(ppi_pwm_off);

    const uint32_t ppi_calibration_lf_capture = ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_END_IDX], &COUNTER_SELECT->TASKS_CAPTURE[COUNTER_CC_CALIBRATION_CAPTURE_LF]);
    const uint32_t ppi_calibration_hf_capture = ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_END_IDX], &COUNTER_SELECT->TASKS_CAPTURE[COUNTER_CC_CALIBRATION_CAPTURE_HF]);
    NRF_PPI->CHG[ppi_group_calibration_capture_lf] = (1 << ppi_calibration_lf_capture);
    NRF_PPI->CHG[ppi_group_calibration_capture_hf] = (1 << ppi_calibration_hf_capture);

    const uint32_t ppi_sample_ready = ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_SAMPLE_END_IDX], &EGU_SELECT->TASKS_TRIGGER[EGU_ACTIVATE_IDX]);
    ppi_fork(ppi_sample_ready, &COUNTER_SELECT->TASKS_CAPTURE[COUNTER_CC_SAMPLE_CAPTURE]);
    NRF_PPI->CHG[ppi_group_sample_activate] = 1 << ppi_sample_ready;
    
    // RTC reset
    (void)ppi_connect(&RTC_SELECT->EVENTS_COMPARE[RTC_CC_RESET_IDX], &RTC_SELECT->TASKS_CLEAR);

    // from measurements, starting and stopping the Timer makes no difference on power consumption
}

static void _calibration_start(struct k_work *work) {
    _calibration_reset();
    k_work_schedule(&_calibration_capture_work, K_SECONDS(_calibration_period));
}

static void _calibration_reset(void) {
    for (int i = 0; i < ARRAY_SIZE(_calibration_buf); i++) {
        _calibration_buf[i] = 0;
    }
    _calibration_buf_idx = 0;
    COUNTER_SELECT->CC[COUNTER_CC_CALIBRATION_CAPTURE_LF] = 2;
    COUNTER_SELECT->CC[COUNTER_CC_CALIBRATION_CAPTURE_HF] = 2;
    _calibration_period = _CALIBRATION_SAMPLE_CAPTURE_PERIOD_INIT_SEC;
}

static void _calibration_capture(struct k_work *work) {
    static const uint32_t CALIBRATION_VAL_RESET = 2;  // if 0, then it will get stuck
    static const size_t CALIBRATION_RANK = 3; // second biggest
    
    // capture calibration and reset. We use LF calibration point by default. But if it is not set (been in HF mode since last calibration), we include HF calibration point
    volatile const uint32_t calibration_point_lf = COUNTER_SELECT->CC[COUNTER_CC_CALIBRATION_CAPTURE_LF];
    volatile const uint32_t calibration_point_hf = COUNTER_SELECT->CC[COUNTER_CC_CALIBRATION_CAPTURE_HF];
    const uint32_t calibration_point_hf_norm = calibration_point_hf * (RTC_TICKS_SAMPLE) / (RTC_TICKS_SAMPLE_HF);

    COUNTER_SELECT->CC[COUNTER_CC_CALIBRATION_CAPTURE_LF] = CALIBRATION_VAL_RESET;
    COUNTER_SELECT->CC[COUNTER_CC_CALIBRATION_CAPTURE_HF] = CALIBRATION_VAL_RESET;

    LOG_DBG("calibration: %d, %d [%d]", calibration_point_lf, calibration_point_hf_norm, calibration_point_hf);
    const uint32_t calibration_consolidate = calibration_point_lf == CALIBRATION_VAL_RESET ? MAX(calibration_point_lf, calibration_point_hf_norm) : calibration_point_lf;
    if (calibration_consolidate <= CALIBRATION_VAL_RESET) {
        LOG_ERR("no new calubration value");
        k_work_schedule(&_calibration_capture_work, K_SECONDS(_calibration_period));
    }
    RETURN_ON_ERR_MSG(calibration_consolidate <= CALIBRATION_VAL_RESET, "no new calibration value");

    // store calibration point, will need at least 3 points to get a valid calibration
    _calibration_buf[_calibration_buf_idx] = calibration_consolidate;
    _calibration_buf_idx = (_calibration_buf_idx + 1) % ARRAY_SIZE(_calibration_buf);
    const uint16_t calibration_filtered = sorted_index_get(_calibration_buf, ARRAY_SIZE(_calibration_buf), CALIBRATION_RANK);

// #if CONFIG_DEBUG
//     // print calibration points
//     printk("Calibrated to %d with period %d from points:", calibration_filtered, _calibration_period);
//     for (uint8_t i = 0; i < ARRAY_SIZE(_calibration_buf); i++) {
//         printk("%s%d", i == _calibration_buf_idx ? "|" : " ", _calibration_buf[i]);
//     }
//     printk("\n");
// #endif

    _counter_region_set(calibration_filtered);

    // schedule next capture, exponentially increasing period
    _calibration_period <<= 1;
    if (_calibration_period > _CALIBRATION_SAMPLE_CAPTURE_PERIOD_MAX_SEC)
        _calibration_period = _CALIBRATION_SAMPLE_CAPTURE_PERIOD_MAX_SEC;
    k_work_schedule(&_calibration_capture_work, K_SECONDS(_calibration_period));
}

static void _egu_irq(void) {
    if (EGU_SELECT->EVENTS_TRIGGERED[EGU_ACTIVATE_IDX]) {
        EGU_SELECT->EVENTS_TRIGGERED[EGU_ACTIVATE_IDX] = 0;
        volatile uint16_t sample = COUNTER_SELECT->CC[COUNTER_CC_SAMPLE_CAPTURE];
        int ret = k_msgq_put(&_samples_msgq, (void*)&sample, K_NO_WAIT);
        LOG_WRN_IF(ret, "msgq full");
        (void)k_work_submit(&_sample_process_work);
    }
}

static void _counter_region_set(uint32_t calibration_point) {
    if (calibration_point == _counter_region.nominal && _counter_region.activate >= 2) return; // second compare to check if uninitialized

    static const uint8_t ACTIVATE_MARGIN = _FIXED8_PERCENT(80);
    static const uint8_t SATURATE_MARGIN = _FIXED8_PERCENT(40);

    uint32_t activate = (calibration_point * ACTIVATE_MARGIN) >> 8;
    uint32_t saturate = (calibration_point * SATURATE_MARGIN) >> 8;

    // minimum value which assures no conflict
    if (activate < 2) {
        activate = 3;
    }
    if (saturate >= activate) {
        saturate = activate - 1;
    }

    _counter_region = (struct _counter_region){
        .nominal = calibration_point, 
        .activate = activate, 
        .saturate = saturate,
    };

    LOG_INF("new regions: nominal: %d, activate: %d, saturate: %d", _counter_region.nominal, _counter_region.activate, _counter_region.saturate);
    COUNTER_SELECT->CC[COUNTER_CC_ACTIVE_TRIGGER] = _counter_region.activate;
}

struct _sample_store {
    uint8_t sample;
    uint8_t filtered;
};

// #define NUM_SAMPLES_STORE 16
// uint32_t _sample_store_idx;
// struct _sample_store _sample_store[NUM_SAMPLES_STORE];

static void _sample_process(struct k_work *work) {
    static uint16_t value_filtered = 0;

    uint16_t sample;
    while (k_msgq_get(&_samples_msgq, &sample, K_NO_WAIT) == 0) {
        if (sample == 0) {
            LOG_WRN("received 0 sample");
            continue;
        }

        if (_state == _STATE_AUTONOMOUS_LOW_FREQUENCY) {
            _set_state(_STATE_HIGH_FREQUENCY);
            k_msgq_purge(&_samples_msgq); // discard all samples, because they are scaled differently in the two modes
            return;
        }

        /* filter value, 1. order low pass */
        static const uint8_t scale_factor = _FIXED8_PERCENT(40);
        value_filtered = (value_filtered*scale_factor + sample*(UINT8_MAX - scale_factor) + 128) >> 8;

        /* map value to something approximately proportional with capacitance, and range 0 to 127 */
        // TODO: precalculate constants
        static const int32_t a = _FIXED8_PERCENT(50);
        const int32_t A = _counter_region.nominal * RTC_TICKS_SAMPLE_HF * a / RTC_TICKS_SAMPLE >> 8;
        const int32_t Sp = _counter_region.activate * RTC_TICKS_SAMPLE_HF / RTC_TICKS_SAMPLE;
        const int32_t Sn = _counter_region.saturate * RTC_TICKS_SAMPLE_HF / RTC_TICKS_SAMPLE;
        const int32_t sample_clamp = CLAMP(value_filtered, (uint16_t)Sn, (uint16_t)Sp);

        const int32_t denominator = (Sp - Sn)*(sample_clamp + A - Sn);
        if (denominator == 0) {
            LOG_WRN("denominator 0");
            continue;
        }
        const int16_t value_transformed = 127*A*(Sp - sample_clamp)/denominator;

        uint16_t data[] = {sample, value_filtered, value_transformed};

        bt_log_notify((uint8_t*)data, sizeof(data));

        // _sample_store[_sample_store_idx] = (struct _sample_store){sample, value_filtered};
        // _sample_store_idx++;
        // if (_sample_store_idx == NUM_SAMPLES_STORE) {
        //     // send
        //     for (int i = 0; i < NUM_SAMPLES_STORE; i++) {
        //         LOG_INF("capd: %d-%d", _sample_store[i].sample, _sample_store[i].filtered);
        //     }
        //     _sample_store_idx = 0;
        // }

        // LOG_INF("S: %d, S_c: %d, V: %d, V_o: %d [%d:%d:%d]", sample, sample_clamp, value, value_filtered, _counter_region.nominal, _counter_region.activate, _counter_region.saturate);
    }

    // if (value_filtered == 0)
    //     _set_state(_STATE_AUTONOMOUS_LOW_FREQUENCY);

    // static uint8_t output_prev = 0;
    // if (output_prev == value_filtered) return;
    // output_prev = value_filtered;
    // _cb(value_filtered);
}
