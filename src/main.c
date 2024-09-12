#include <zephyr/kernel.h>

#include "cap_touch/cap_touch.h"
#include "services/hardware_spec.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
        const struct hardware_spec* hw_spec = hardware_spec_get();
        LOG_INF("starting with HW: %s", hw_spec->DIS_hw_rev_str);
        cap_touch_init();

        cap_touch_start();

        k_sleep(K_FOREVER);
        return 0;
}
