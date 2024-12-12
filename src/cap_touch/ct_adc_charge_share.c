#include "cap_touch.h"

#include "nrf.h"
#include <zephyr/kernel.h>

#include "utils/macros_common.h"
#include "utils/ppi_connect.h"
#include "bluetooth/bt_log.h"
#include "io/led.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_DBG);

static void _adc_isr(void);

#define RTC_SEL NRF_RTC2
#define RTC_SEL_IRQn RTC2_IRQn

void cap_touch_init(cap_touch_event_t event_cb, uint32_t psel_comp, uint32_t psel_pin) {
    ARG_UNUSED(psel_pin);
    ARG_UNUSED(event_cb);

    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_14bit;
    // static const uint32_t RESOLUTION = 16384;

    NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;

    /* set pin */
    NRF_SAADC->CH[0].PSELP = psel_comp + 1; // adc psel 1 offset from comp psel

    /* charge the capacitors while sampling, thus the output value corelates with the aquisition time */
    #define SAADC_CH_CONFIG_TACQ_1us 6
    #define SAADC_CH_CONFIG_TACQ_2us 7
    #define TACQ_SELECT SAADC_CH_CONFIG_TACQ_3us

    NRF_SAADC->CH[0].CONFIG = (SAADC_CH_CONFIG_RESP_Pullup << SAADC_CH_CONFIG_RESP_Pos) |
                              (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
                              (SAADC_CH_CONFIG_REFSEL_VDD1_4 << SAADC_CH_CONFIG_REFSEL_Pos) |
                              (TACQ_SELECT << SAADC_CH_CONFIG_TACQ_Pos) |
                              (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos);

    /* set up limits to trigger the CPU when a new interesting value occurs */
    // NRF_SAADC->CH[0].LIMIT = ((RESOLUTION / 2) << SAADC_CH_LIMIT_LOW_Pos) & SAADC_CH_LIMIT_LOW_Msk |
    //                          SAADC_CH_LIMIT_HIGH_Msk;
    // NRF_SAADC->INTENSET = SAADC_INTENSET_CH0LIMITL_Msk;

#if CONFIG_DEBUG
    NRF_SAADC->INTENSET = SAADC_INTENSET_RESULTDONE_Msk;
#endif

    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;

    // Calibrate the SAADC (only needs to be done once in a while)
    NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
    while (NRF_SAADC->EVENTS_CALIBRATEDONE == 0) {}
    NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
    while (NRF_SAADC->STATUS == (SAADC_STATUS_STATUS_Busy <<SAADC_STATUS_STATUS_Pos)) {}

    IRQ_CONNECT(SAADC_IRQn, 3, _adc_isr, 0, 0);
    irq_enable(SAADC_IRQn);

    // /* Set up RTC to trigger ADC */
    RTC_SEL->PRESCALER = 4095; // use prescalar to set operating frequency
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

static void _adc_isr(void) {
    // if (NRF_SAADC->EVENTS_CH[0].LIMITL) {
    //     NRF_SAADC->EVENTS_CH[0].LIMITL = 0;
    //     #define SAADC_RESULT_REG ((volatile uint32_t*)(NRF_SAADC_BASE + 0x5EC))
    //     const uint32_t regresult = *SAADC_RESULT_REG;
    //     printk("LIMIT result: %d\n", regresult);
    // }

#if CONFIG_DEBUG
    if (NRF_SAADC->EVENTS_RESULTDONE) {
        NRF_SAADC->EVENTS_RESULTDONE = 0;
        #define SAADC_RESULT_REG ((volatile uint32_t*)(NRF_SAADC_BASE + 0x5EC))
        const uint32_t regresult = *SAADC_RESULT_REG;
        uint16_t buf[] = {(uint16_t)regresult};
        bt_log_notify((uint8_t*)buf, sizeof(buf));
        printk("%d\n", regresult);
    }
#endif
}