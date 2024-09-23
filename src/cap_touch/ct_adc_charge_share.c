#include "cap_touch.h"

#include "nrf.h"
#include <zephyr/kernel.h>

#include "services/hardware_spec.h"
#include "utils/macros_common.h"
#include "utils/ppi_connect.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_DBG);

static volatile int16_t _sample_result;
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

    _pin = adc_pin;
    _psel = adc_psel;

    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_10bit;
    static const uint32_t RESOLUTION = 1024;

    NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;

    /* set pin */
    NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELN_PSELN_NC;

    /*
    * GPIO charges pin to VDD. Sampled voltage is a scalor of VDD, and we must therefore use VDD reference
    * For now we have unity gain (ref & gain together)
    */
    NRF_SAADC->CH[0].CONFIG = (SAADC_CH_CONFIG_RESP_Bypass << SAADC_CH_CONFIG_RESP_Pos) |
                              (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
                              (SAADC_CH_CONFIG_REFSEL_VDD1_4 << SAADC_CH_CONFIG_REFSEL_Pos) |
                              (SAADC_CH_CONFIG_TACQ_3us << SAADC_CH_CONFIG_TACQ_Pos) |
                              (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos);

    NRF_SAADC->CH[0].LIMIT = ((RESOLUTION / 2) << SAADC_CH_LIMIT_HIGH_Pos) & SAADC_CH_LIMIT_HIGH_Msk;

    /* Enable interrupt on threshold */
    NRF_SAADC->INTENSET = SAADC_INTENSET_CH0LIMITH_Msk | SAADC_INTENSET_RESULTDONE_Msk;

    NRF_SAADC->RESULT.MAXCNT = 1;
    NRF_SAADC->RESULT.PTR = (uint32_t)&_sample_result;

    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;

    // Calibrate the SAADC (only needs to be done once in a while)
    NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
    while (NRF_SAADC->EVENTS_CALIBRATEDONE == 0) {}
    NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
    while (NRF_SAADC->STATUS == (SAADC_STATUS_STATUS_Busy <<SAADC_STATUS_STATUS_Pos)) {}

    IRQ_CONNECT(SAADC_IRQn, 3, _adc_isr, 0, 0);
    irq_enable(SAADC_IRQn);


    /* gpio task to set pin to 1 or Z */
    NRF_GPIO->PIN_CNF[adc_pin] = 
        (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
        | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
        | (GPIO_PIN_CNF_DRIVE_D0S1 << GPIO_PIN_CNF_DRIVE_Pos)
        | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
        ;

    //  NRF_GPIOTE->CONFIG[0] = 
    //     (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos)
    //     | ((adc_pin) << GPIOTE_CONFIG_PSEL_Pos)
    //     | (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos)
    //     ;

    /* Set up RTC with CC0 and CC1 */
    NRF_RTC0->PRESCALER = 25; // use prescalar to set operating frequency, default 5
    NRF_RTC0->CC[0] = 1; // PWM off, results in on time of 5/32768 = 150µs.
    NRF_RTC0->CC[1] = 800; // PWM on, total period of 800*5/32768 = 122ms
    NRF_RTC0->EVTENSET = RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos
        | RTC_EVTEN_COMPARE1_Enabled << RTC_EVTEN_COMPARE1_Pos
        ;

#ifdef CONFIG_DEBUG
    // interrupt to log value
    NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE0_Msk | RTC_INTENSET_COMPARE1_Msk
        ; // configure interrupt to log value
    IRQ_CONNECT(RTC0_IRQn, 3, _rtc_isr, 0, 0);
    irq_enable(RTC0_IRQn);
#endif

    /* connect peripherals together */
    // const unsigned int sample_ppi = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[0], (uint32_t)&NRF_GPIOTE->TASKS_CLR[0]);
    // ppi_fork(sample_ppi, (uint32_t)&NRF_SAADC->TASKS_SAMPLE);

    // const unsigned int reset_ppi = ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[1], (uint32_t)&NRF_GPIOTE->TASKS_SET[0]);
    // ppi_fork(reset_ppi, (uint32_t)&NRF_RTC0->TASKS_CLEAR);
    ppi_connect((uint32_t)&NRF_RTC0->EVENTS_COMPARE[1], (uint32_t)&NRF_RTC0->TASKS_CLEAR);

    LOG_DBG("cap_touch_init done");
}

void cap_touch_start(void) {
    LOG_DBG("cap_touch_start");
    NRF_RTC0->TASKS_START = 1;
}

static void _adc_isr(void) {
    if (NRF_SAADC->EVENTS_CH[0].LIMITH) {
        NRF_SAADC->EVENTS_CH[0].LIMITH = 0;
        printk("LIMITH: %d\n", _sample_result);
    }

    if (NRF_SAADC->EVENTS_RESULTDONE) {
        NRF_SAADC->EVENTS_RESULTDONE = 0;

        printk("result: %d\n", _sample_result);
    }
}
 
static void _rtc_isr(void) {
    if (NRF_RTC0->EVENTS_COMPARE[0]) {
        NRF_RTC0->EVENTS_COMPARE[0] = 0;
        printk("RTC adc start\n");
        NRF_GPIO->OUTCLR = 1 << _pin;
        NRF_SAADC->CH[0].PSELP = _psel;
        NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;
        NRF_SAADC->TASKS_SAMPLE = 1;
    }

    if (NRF_RTC0->EVENTS_COMPARE[1]) {
        NRF_RTC0->EVENTS_COMPARE[1] = 0;
        printk("RTC gpio charge\n");
        NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELN_PSELN_NC;
        NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Disabled;
        NRF_GPIO->OUTSET = 1 << _pin;
    }
}