#include "cap_touch_discharge.h"

#include <zephyr/kernel.h>

#include "services/hardware_spec.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cap_touch, LOG_LEVEL_INF);

/** **STATE MACHINE**
 * - EVENT COMP low TH -> COMP DISCONNECTED, charging up cap, enabling high TH
 * - EVENT COMP high TH -> COMP CONNECTED, discharging cap (S1D0), enabling low TH
 * 
 * Details:
 * - COMP low TH -> COMP DISCONNECTED : 
 * set pin to NC, this will give back the pin to the GPIO
 * set up a timer to let the pin charge up. 
 */

static void _comparator_configure(void);
static void _timer_configure(void);
static void _peripheral_connect(void);

void cap_touch_discharge_init(void) {
    LOG_INF("cap_touch_init");

    const struct hardware_spec* hw_spec = hardware_spec_get();
    RETURN_ON_ERR_MSG(hw_spec->cap_touch_psel == SAADC_CH_PSELP_PSELP_NC, "no cap touch analog pin");

    const struct device* port = common_port;

    uint32_t cap_pin = hardware_spec_pin_get(HARDWARE_SPEC_PIN_CAPTOUCH);
    RETURN_ON_ERR_MSG(cap_pin < 0, "no cap touch pin");

    _comparator_configure();
    _timer_configure();
    _peripheral_connect();
}

static void _comparator_configure(void) {

}

static void _timer_configure(void) {

}

static void _peripheral_connect(void) {
    /* PPI connection between RTC (sampling period) to COMP */

    /* PPI connection between  */
}
