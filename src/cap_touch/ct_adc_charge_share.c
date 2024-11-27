#include "cap_touch.h"

#include "nrf.h"
#include <zephyr/kernel.h>

#include "services/hardware_spec.h"
#include "utils/macros_common.h"
#include "utils/ppi_connect.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_DBG);

static volatile uint32_t _sample;
static int _pin;
static int _psel;

static void _rtc_isr(void);
static void _adc_isr(void);

void cap_touch_init(void) {

    const struct hardware_spec* hw_spec = hardware_spec_get();
    const int adc_pin = hardware_spec_pin_get(HARDWARE_SPEC_PIN_CAPTOUCH);
    RETURN_ON_ERR_MSG(adc_pin == -1, "no cap touch pin");
    int adc_psel = hw_spec->cap_touch_psel;
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");
    adc_psel += 1; // COMP psel value is one less than ADC pin value

    LOG_INF("cap touch pin select: %d, psel: %d", adc_pin, adc_psel);
 
    _pin = adc_pin;
    _psel = adc_psel;

    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_14bit;
    static const uint32_t RESOLUTION = 16384;

    NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;

    /* set pin */
    NRF_SAADC->CH[0].PSELP = adc_psel;

    /*
    * GPIO charges pin to VDD. Sampled voltage is a scalor of VDD, and we must therefore use VDD reference
    * For now we have unity gain (ref & gain together)
    */
    #define SAADC_CH_CONFIG_TACQ_1us 6
    #define SAADC_CH_CONFIG_TACQ_2us 7
    NRF_SAADC->CH[0].CONFIG = (SAADC_CH_CONFIG_RESP_Bypass << SAADC_CH_CONFIG_RESP_Pos) |
                              (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
                              (SAADC_CH_CONFIG_REFSEL_VDD1_4 << SAADC_CH_CONFIG_REFSEL_Pos) |
                              (SAADC_CH_CONFIG_TACQ_3us << SAADC_CH_CONFIG_TACQ_Pos) |
                              (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos);

    NRF_SAADC->CH[0].LIMIT = ((RESOLUTION / 2) << SAADC_CH_LIMIT_LOW_Pos) & SAADC_CH_LIMIT_LOW_Msk |
                             SAADC_CH_LIMIT_HIGH_Msk;

    /* Enable interrupt on threshold */
    // NRF_SAADC->INTENSET = SAADC_INTENSET_CH0LIMITL_Msk;

#if CONDIG_DEBUG
    NRF_SAADC->INTENSET = SAADC_INTENSET_RESULTDONE_Msk;
#endif

    NRF_SAADC->RESULT.MAXCNT = 1;
    NRF_SAADC->RESULT.PTR = (uint32_t)&_sample;

    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;

    // Calibrate the SAADC (only needs to be done once in a while)
    NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
    while (NRF_SAADC->EVENTS_CALIBRATEDONE == 0) {}
    NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
    while (NRF_SAADC->STATUS == (SAADC_STATUS_STATUS_Busy <<SAADC_STATUS_STATUS_Pos)) {}

    // IRQ_CONNECT(SAADC_IRQn, 3, _adc_isr, 0, 0);
    // irq_enable(SAADC_IRQn);


    /* Set up RTC to trigger ADC */
    NRF_RTC0->PRESCALER = 3276; // use prescalar to set operating frequency
    NRF_RTC0->EVTENSET = RTC_EVTEN_TICK_Enabled << RTC_EVTEN_TICK_Pos;
        ;
    ppi_connect((uint32_t)&NRF_RTC0->EVENTS_TICK, (uint32_t)&NRF_SAADC->TASKS_SAMPLE);

    LOG_DBG("cap_touch_init done");
}

void cap_touch_start(void) {
    LOG_DBG("cap_touch_start");
    NRF_RTC0->TASKS_START = 1;
}

// static void _adc_isr(void) {
//     if (NRF_SAADC->EVENTS_CH[0].LIMITL) {
//         NRF_SAADC->EVENTS_CH[0].LIMITL = 0;
//         #define SAADC_RESULT_REG ((volatile uint32_t*)(NRF_SAADC_BASE + 0x5EC))
//         const uint32_t regresult = *SAADC_RESULT_REG;
//         printk("LIMIT result: %d\n", regresult);
//     }

// #if CONDIG_DEBUG
//     if (NRF_SAADC->EVENTS_RESULTDONE) {
//         NRF_SAADC->EVENTS_RESULTDONE = 0;
//         #define SAADC_RESULT_REG ((volatile uint32_t*)(NRF_SAADC_BASE + 0x5EC))
//         const uint32_t regresult = *SAADC_RESULT_REG;
//         printk("result: %d\n", regresult);
//     }
// #endif
// }