/*
 * File: hardware_spec.h
 * Author: Rein Gundersen Bentdal
 * Created: 19.Des 2024
 *
 * Copyright (c) 2024, Rein Gundersen Bentdal
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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