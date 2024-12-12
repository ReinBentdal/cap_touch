#pragma once

#include <stdint.h>

typedef void (*cap_touch_event_t)(uint8_t value);

void cap_touch_init(cap_touch_event_t event, uint32_t psel_comp, uint32_t psel_pin);

void cap_touch_start(void);