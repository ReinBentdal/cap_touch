/// This module implementes cap touch using ISOURCE to create an oscillation.
///
/// 

#include "cap_touch_oscillate.h"

#include <zephyr/kernel.h>
#include "nrf.h"

#include "services/hardware_spec.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch_oscillate, LOG_LEVEL_INF);

#define Vref_mV (3000) // in reality it varies, but the exact value should not be too important
#define _COMP_TH_CALC(mV) ((64*mV/Vref_mV) -1)

static void _comparator_configure(void);

void cap_touch_oscillate_init(void) {
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    const struct device* port = common_port;

    uint32_t cap_pin = hardware_spec_pin_get(HARDWARE_SPEC_PIN_CAPTOUCH);
    RETURN_ON_ERR_MSG(cap_pin < 0, "no cap touch pin");

    _comparator_configure();
}

static void _comparator_configure(void) {
    // set voltage reference to VDD
    NRF_COMP->REFSEL = COMP_REFSEL_REFSEL_VDD << COMP_REFSEL_REFSEL_Pos;

    // set TH for up and down
    NRF_COMP->TH = (_COMP_TH_CALC(2000) << COMP_TH_THUP_Pos) | (_COMP_TH_CALC(500) << COMP_TH_THDOWN_Pos);

    // set MODE, single ended and low power
    NRF_COMP->MODE = (COMP_MODE_MAIN_SE << COMP_MODE_MAIN_Pos) | (COMP_MODE_SP_Low << COMP_MODE_SP_Pos);

    // set COMP to trigger on DOWN event
    NRF_COMP->INTENSET = COMP_INTEN_DOWN_Enabled << COMP_INTEN_DOWN_Pos;

    // when DOWN trigger, set to stop COMP
    NRF_COMP->SHORTS = COMP_SHORTS_DOWN_STOP_Enabled << COMP_SHORTS_DOWN_STOP_Pos;

    // enable current source
    NRF_COMP->ISOURCE = COMP_ISOURCE_ISOURCE_Ien2mA5 << COMP_ISOURCE_ISOURCE_Pos;

    // configure DOWN interrupt to call a function
    NVIC_EnableIRQ(COMP_LPCOMP_IRQn);
    NVIC_SetPriority(COMP_LPCOMP_IRQn, 3);
}

void COMP_Handler(void) {
    LOG_INF("COMP_Handler");
    // stop COMP
    NRF_COMP->ENABLE = 0;
}