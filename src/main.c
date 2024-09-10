#include <zephyr/kernel.h>

#include "io/cap_touch_discharge.h"

int main(void)
{
        cap_touch_discharge_init();
        return 0;
}
