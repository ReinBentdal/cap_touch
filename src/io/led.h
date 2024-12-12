#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>
#include "nrf.h"

void led_init(uint32_t pin, volatile NRF_GPIO_Type* port, int polarity);
void led_blink(void);

#endif