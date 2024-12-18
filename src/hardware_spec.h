#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <nrf.h>

// in this sample, everything is assumed to be on a single common GPIO
#define PORT_COMMON NRF_P0

// select which LED to use
#define LED_PIN 25
#define LED_POLARITY 1

// which pin the cap electride is connected to
#define CAPTOUCH_PSEL_COMP COMP_PSEL_PSEL_AnalogInput7
#define CAPTOUCH_PSEL_PIN 31