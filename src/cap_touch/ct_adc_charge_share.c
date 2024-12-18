#include "cap_touch.h"

#include "nrf.h"
#include <zephyr/kernel.h>

#include "utils/macros_common.h"
#include "utils/ppi_connect.h"

#if CONFIG_DEBUG
#include "io/led.h"
#include "bluetooth/bt_log.h"
static void _adc_isr(void);
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_DBG);

#define RTC_SEL NRF_RTC2

static volatile uint32_t _sample;

void cap_touch_init(cap_touch_event_t event_cb, uint32_t psel_comp, uint32_t psel_pin) {
    ARG_UNUSED(event_cb);
    ARG_UNUSED(psel_pin);
    const uint32_t psel_adc = psel_comp + 1; // adc psel 1 offset from comp psel

    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_8bit;
    // static const uint32_t RESOLUTION = 16384;

    NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;
    NRF_SAADC->SAMPLERATE = SAADC_SAMPLERATE_MODE_Task << SAADC_SAMPLERATE_MODE_Pos;

    /* set pin */
    NRF_SAADC->CH[0].PSELP = psel_adc;

    /* charge the capacitors while sampling, thus the output value corelates with the aquisition time */
    #define SAADC_CH_CONFIG_TACQ_1us 6
    #define SAADC_CH_CONFIG_TACQ_2us 7
    #define TACQ_SELECT SAADC_CH_CONFIG_TACQ_10us

    NRF_SAADC->CH[0].CONFIG = (SAADC_CH_CONFIG_RESP_Pullup << SAADC_CH_CONFIG_RESP_Pos) |
                              (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
                              (SAADC_CH_CONFIG_REFSEL_VDD1_4 << SAADC_CH_CONFIG_REFSEL_Pos) |
                              (TACQ_SELECT << SAADC_CH_CONFIG_TACQ_Pos) |
                              (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos);

    // TODO: 
    /* set up limits to trigger the CPU when a new interesting value occurs */
    // NRF_SAADC->CH[0].LIMIT = ((RESOLUTION / 2) << SAADC_CH_LIMIT_LOW_Pos) & SAADC_CH_LIMIT_LOW_Msk |
    //                          SAADC_CH_LIMIT_HIGH_Msk;
    // NRF_SAADC->INTENSET = SAADC_INTENSET_CH0LIMITL_Msk;


#if CONFIG_DEBUG
    NRF_SAADC->INTENSET = SAADC_INTENSET_RESULTDONE_Msk;
    IRQ_CONNECT(SAADC_IRQn, 3, _adc_isr, 0, 0);
    irq_enable(SAADC_IRQn);
#endif

    // dummy
    // NRF_SAADC->RESULT.MAXCNT = 2;
    // NRF_SAADC->RESULT.PTR = (uint32_t)&_sample;

    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;
    NRF_SAADC->TASKS_STOP = 1;

    // Calibrate the SAADC (only needs to be done once in a while)
    // NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
    // while (NRF_SAADC->EVENTS_CALIBRATEDONE == 0) {}
    // NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
    // while (NRF_SAADC->STATUS == (SAADC_STATUS_STATUS_Busy <<SAADC_STATUS_STATUS_Pos)) {}

    // /* Set up RTC to trigger ADC */
    RTC_SEL->PRESCALER = 3999; // use prescalar to set operating frequency
    RTC_SEL->EVTENSET = RTC_EVTEN_TICK_Msk;

    ppi_connect(&RTC_SEL->EVENTS_TICK, &NRF_SAADC->TASKS_SAMPLE);
}

void cap_touch_start(void) {
    LOG_DBG("cap_touch_start");
    RTC_SEL->TASKS_START = 1;
}

void cap_touch_stop(void) {
    LOG_DBG("cap_touch_stop");
    RTC_SEL->TASKS_STOP = 1;
}

#if CONFIG_DEBUG
static void _adc_isr(void) {
    // if (NRF_SAADC->EVENTS_CH[0].LIMITL) {
    //     NRF_SAADC->EVENTS_CH[0].LIMITL = 0;
    //     #define SAADC_RESULT_REG ((volatile uint32_t*)(NRF_SAADC_BASE + 0x5EC))
    //     const uint32_t regresult = *SAADC_RESULT_REG;
    //     printk("LIMIT result: %d\n", regresult);
    // }

    // TODO: when limits implemented, remove this
    if (NRF_SAADC->EVENTS_RESULTDONE) {
        NRF_SAADC->EVENTS_RESULTDONE = 0;
        #define SAADC_RESULT_REG ((volatile uint32_t*)(NRF_SAADC_BASE + 0x5EC))
        const uint32_t regresult = *SAADC_RESULT_REG;
        uint16_t buf[] = {(uint16_t)regresult};
        bt_log_notify((uint8_t*)buf, sizeof(buf));
    }
}
#endif